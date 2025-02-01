//
// Copyright (c) 2024 Weongyo Jeong <weongyo@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

import Alamofire
import SwiftUI
import SwiftyJSON

class UiEnrollmentNewModel : ObservableObject {
    var mEnrollmentToken: String = ""
    var mDeviceName: String = ""
    var mEnrollmentSecret: String = ""

    struct enroll_params: Encodable {
        let token: String
        let name: String
        let wireguard_pubkey: String
        let secret: String
    }

    struct enroll_decodable_type: Decodable {
        let status: Int
        let msg: String
        let sso_url: String
    }
    
    private func enroll_set_result(target: UiEnrollmentNewView, status: Int, msg: String) {
        if status != 200 {
            if status == 301 {
                target.mMfaAlertNeed = true
                target.mMfaAlertURL = msg
            } else {
                target.mErrorAlertNeed = true
                target.mErrorAlertMessage = msg
            }
        }
    }
    
    func enroll_success(target: UiEnrollmentNewView,
                        appModel: AppModel, priv_key: String, raw_str: String) {
        let r = mudband_ui_enroll_post(priv_key, raw_str)
        if r != 0 {
            mudband_ui_log(0, "BANDEC_00277: mudband_ui_enroll() failed.")
            return
        }
        DispatchQueue.main.async {
            appModel.update_enrollments()
            target.dismiss()
        }
    }
    
    private func update_is_enrolling(target: UiEnrollmentNewView, ing: Bool) {
        target.mIsEnrolling = ing
    }
    
    func enroll(target: UiEnrollmentNewView, appModel: AppModel) async {
        let keys = mudband_ui_create_wireguard_keys()
        if keys == nil || keys?.count != 2 {
            enroll_set_result(target: target,
                              status: 500,
                              msg: "BANDEC_00278: mudband_ui_create_wireguard_keys() failed.")
            return
        }
        let pub_key: String = keys![0] as! String
        let priv_key: String = keys![1] as! String
        self.update_is_enrolling(target: target, ing: true)
        let parameters = enroll_params(token: mEnrollmentToken,
                                       name: mDeviceName,
                                       wireguard_pubkey: pub_key,
                                       secret: mEnrollmentSecret)
        AF.request("https://www.mud.band/api/band/enroll",
                   method: .post,
                   parameters: parameters,
                   encoder: JSONParameterEncoder.default,
                   interceptor: .retryPolicy)
        .responseString { resp in
            self.update_is_enrolling(target: target, ing: false)
            switch resp.result {
            case .success(let str):
                if let obj = try? JSON(data: Data(str.utf8)) {
                    if obj["status"].intValue != 200 {
                        if obj["status"].intValue == 301 {
                            self.enroll_set_result(target: target,
                                                   status: 301,
                                                   msg: obj["sso_url"].stringValue)
                            return
                        }
                        self.enroll_set_result(target: target,
                                               status: 501,
                                               msg: obj["msg"].stringValue)
                        return
                    }
                }
                self.enroll_success(target: target,
                                    appModel: appModel, priv_key: priv_key, raw_str: str)
            case .failure(let error):
                self.enroll_set_result(target: target,
                                       status: 502,
                                       msg: "\(error)")
            }
        }
        enroll_set_result(target: target, status: 200, msg: "")
    }
}

struct UiEnrollmentNewView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @ObservedObject private var mEnrollmentModel = UiEnrollmentNewModel()
    @Environment(\.dismiss) var dismiss
    @Environment(\.openURL) var openURL
    @State var mMfaAlertNeed = false
    @State var mMfaAlertURL = ""
    @State var mErrorAlertNeed = false
    @State var mErrorAlertMessage = ""
    @State var mIsEnrolling = false

    @ViewBuilder
    var body: some View {
        NavigationStack {
            VStack {
                Text("Input the enrollment information.")
                    .padding(.bottom, 10)
                Form {
                    TextField("Enrollment Token", text: $mEnrollmentModel.mEnrollmentToken)
                    TextField("Device Name", text: $mEnrollmentModel.mDeviceName)
                    TextField("Enrollment Secret", text: $mEnrollmentModel.mEnrollmentSecret)
                }
                HStack {
                    Spacer()
                    if mIsEnrolling {
                        ProgressView().controlSize(.small)
                            .padding(.trailing, 2)
                    }
                    Button("Enroll") {
                        Task {
                            await mEnrollmentModel.enroll(target: self,
                                                          appModel: mAppModel)
                        }
                    }
                    .disabled(mIsEnrolling == true)
                    .alert("Enrollment Error", isPresented: $mErrorAlertNeed) {
                        Button("Dismiss", role: .cancel) { }
                    } message: {
                        Text(mErrorAlertMessage)
                    }
                    .alert("MFA Required", isPresented: $mMfaAlertNeed) {
                        Button("Open URL") {
                            if let url = URL(string: mMfaAlertURL) {
                                openURL(url)
                            }
                        }
                        Button("Dismiss", role: .cancel) {
                            
                        }
                    }message: {
                        Text("MFA required to enroll")
                    }
                }
                .padding()
            }
            .navigationTitle("Enrollment")
        }
    }
}

#Preview {
    UiEnrollmentNewView()
}
