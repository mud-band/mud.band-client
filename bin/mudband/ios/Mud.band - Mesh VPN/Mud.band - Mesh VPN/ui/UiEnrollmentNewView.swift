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
                        appModel: AppModel, priv_key: String, obj: JSON, raw_str: String) {
        let r = mudband_ui_enroll_post(priv_key, raw_str)
        if r != 0 {
            mudband_ui_log(0, "BANDEC_00277: mudband_ui_enroll() failed.")
            return
        }
        
        let isPublic = obj["band"]["opt_public"].intValue
        DispatchQueue.main.async {
            target.mIsPublicBand = isPublic == 1
            target.mShowSuccessAlert = true
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
        let resp = await AF.request("https://www.mud.band/api/band/enroll",
                                    method: .post,
                                    parameters: parameters,
                                    encoder: JSONParameterEncoder.default,
                                    interceptor: .retryPolicy)
            .serializingString().response
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
                self.enroll_success(target: target,
                                    appModel: appModel, priv_key: priv_key,
                                    obj: obj,
                                    raw_str: str)
            } else {
                self.enroll_set_result(target: target,
                                       status: 503,
                                       msg: "Failed to parse to JSON object.")
            }
        case .failure(let error):
            self.enroll_set_result(target: target,
                                   status: 502,
                                   msg: "\(error)")
        }
        self.enroll_set_result(target: target, status: 200, msg: "")
    }
}

struct UiEnrollmentNewView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @ObservedObject private var mEnrollmentModel = UiEnrollmentNewModel()
    @Environment(\.dismiss) var dismiss
    @Environment(\.openURL) var openURL
    @Environment(\.colorScheme) var colorScheme
    @State var mMfaAlertNeed = false
    @State var mMfaAlertURL = ""
    @State var mErrorAlertNeed = false
    @State var mErrorAlertMessage = ""
    @State var mIsEnrolling = false
    @State var mShowSuccessAlert = false
    @State var mIsPublicBand = false

    @ViewBuilder
    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                VStack(spacing: 8) {
                    Image(systemName: "network")
                        .font(.system(size: 45))
                        .foregroundStyle(.blue)
                        .padding(.top, 10)
                    
                    Text("Device Enrollment")
                        .font(.headline)
                        .fontWeight(.bold)
                    
                    Text("Connect your device to the mesh network by entering your enrollment information.")
                        .font(.caption)
                        .multilineTextAlignment(.center)
                        .foregroundStyle(.secondary)
                        .padding(.horizontal, 16)
                        .padding(.bottom, 5)
                }
                .padding(.vertical, 8)
                
                Form {
                    Section(header: Text("ENROLLMENT DETAILS").font(.caption)) {
                        HStack {
                            Image(systemName: "key.fill")
                                .foregroundStyle(.blue)
                                .frame(width: 20)
                            TextField("Enrollment Token", text: $mEnrollmentModel.mEnrollmentToken)
                                .textInputAutocapitalization(.never)
                                .autocorrectionDisabled()
                        }
                        .padding(.vertical, 2)
                        
                        HStack {
                            Image(systemName: "display")
                                .foregroundStyle(.blue)
                                .frame(width: 20)
                            TextField("Device Name", text: $mEnrollmentModel.mDeviceName)
                        }
                        .padding(.vertical, 2)
                        
                        HStack {
                            Image(systemName: "lock.fill")
                                .foregroundStyle(.blue)
                                .frame(width: 20)
                            SecureField("Enrollment Secret (Optional)", text: $mEnrollmentModel.mEnrollmentSecret)
                                .textInputAutocapitalization(.never)
                                .autocorrectionDisabled()
                        }
                        .padding(.vertical, 2)
                    }
                    .listRowBackground(colorScheme == .dark ? Color(UIColor.secondarySystemBackground) : Color(UIColor.systemBackground))
                }
                .scrollContentBackground(.hidden)
                
                Button {
                    Task {
                        await mEnrollmentModel.enroll(target: self, appModel: mAppModel)
                    }
                } label: {
                    HStack {
                        if mIsEnrolling {
                            ProgressView()
                                .padding(.trailing, 5)
                        }
                        Text(mIsEnrolling ? "Enrolling..." : "Enroll Device")
                            .fontWeight(.semibold)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(10)
                }
                .disabled(mIsEnrolling)
                .padding(.horizontal)
                .padding(.vertical, 10)
            }
            .navigationTitle("Enrollment")
            .navigationBarTitleDisplayMode(.inline)
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
                Button("Cancel", role: .cancel) { }
            } message: {
                Text("Multi-factor authentication is required to complete the enrollment process.")
            }
            .alert("Enrollment successful", isPresented: $mShowSuccessAlert) {
                Button("Okay") {
                    DispatchQueue.main.async {
                        mAppModel.update_enrollments()
                        dismiss()
                    }
                }
            } message: {
                if mIsPublicBand {
                    Text("NOTE: This band is public. This means that:\n\n• Nobody can connect to your device without your permission.\n• Your default policy is 'block'.\n• You need to add an ACL rule to allow the connection.\n• To control ACL, you need to open the WebCLI.\n\nFor details, please visit: https://mud.band/docs/public-band")
                        .multilineTextAlignment(.leading)
                } else {
                    Text("NOTE: This band is private. This means that:\n\n• Band admin only can control ACL rules and the default policy.\n• You can't control your device.\n\nFor details, please visit: https://mud.band/docs/private-band")
                        .multilineTextAlignment(.leading)
                }
            }
        }
    }
}

#Preview {
    UiEnrollmentNewView()
}
