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

import AlertToast
import Alamofire
import SwiftUI
import SwiftyJSON

class UiEnrollmentNewModel : ObservableObject {
    var mEnrollmentToken: String = ""
    var mDeviceName: String = ""
    var mEnrollmentSecret: String = ""
    
    @Published var showSuccessAlert: Bool = false
    @Published var isPublicBand: Bool = false

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
                target.mErrorAlertMessage = msg
                target.mShowErrorToast = true
            }
        }
    }
    
    func enroll_success(target: UiEnrollmentNewView, appModel: AppModel,
                        priv_key: String, obj: JSON, raw_str: String) {
        let r = mudband_ui_enroll_post(priv_key, raw_str)
        if r != 0 {
            mudband_ui_log(0, "BANDEC_00414: mudband_ui_enroll() failed.")
            return
        }
        
        let isPublic = obj["band"]["opt_public"].intValue == 1
        DispatchQueue.main.async {
            target.mIsPublicBand = isPublic
            target.mShowEnrollmentSuccessAlert = true
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
                              msg: "BANDEC_00415: mudband_ui_create_wireguard_keys() failed.")
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
    @State var mMfaAlertNeed = false
    @State var mMfaAlertURL = ""
    @State var mErrorAlertNeed = false
    @State var mErrorAlertMessage = ""
    @State var mIsEnrolling = false
    @State var mShowSuccessToast = false
    @State var mShowErrorToast = false
    
    @State var mShowEnrollmentSuccessAlert = false
    @State var mIsPublicBand = false
    @State var mShowLearnMoreSheet = false
    @State var mLearnMoreURL = ""

    @ViewBuilder
    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            VStack(alignment: .leading, spacing: 8) {
                Text("Device Enrollment")
                    .font(.title)
                    .fontWeight(.bold)
                    .foregroundColor(.primary)
                
                Text("Enter the information to register your new device.")
                    .font(.body)
                    .foregroundColor(.secondary)
            }
            .padding(.horizontal)
            
            VStack(spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Enrollment Token")
                        .font(.headline)
                        .foregroundColor(.primary)
                    
                    TextField("Enter your enrollment token", text: $mEnrollmentModel.mEnrollmentToken)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .frame(maxWidth: .infinity)
                }
                
                VStack(alignment: .leading, spacing: 8) {
                    Text("Device Name")
                        .font(.headline)
                        .foregroundColor(.primary)
                    
                    TextField("Enter a name for this device", text: $mEnrollmentModel.mDeviceName)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .frame(maxWidth: .infinity)
                }
                
                VStack(alignment: .leading, spacing: 8) {
                    Text("Enrollment Secret (Optional)")
                        .font(.headline)
                        .foregroundColor(.primary)
                    
                    TextField("Enter your secret if you have one", text: $mEnrollmentModel.mEnrollmentSecret)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .frame(maxWidth: .infinity)
                }
            }
            .padding()
            
            Spacer()
            
            HStack {
                Button(action: {
                    dismiss()
                }) {
                    Text("Back")
                        .frame(minWidth: 100)
                        .padding(.vertical, 6)
                        .padding(.horizontal, 12)
                }
                .buttonStyle(.bordered)
                .controlSize(.regular)
                .disabled(mIsEnrolling)
                
                Spacer()
                
                Button(action: {
                    Task {
                        await mEnrollmentModel.enroll(target: self, appModel: mAppModel)
                    }
                }) {
                    HStack {
                        if mIsEnrolling {
                            ProgressView()
                                .controlSize(.small)
                                .padding(.trailing, 4)
                        }
                        
                        Text(mIsEnrolling ? "Enrolling..." : "Enroll Device")
                    }
                    .frame(minWidth: 100)
                    .padding(.vertical, 6)
                    .padding(.horizontal, 12)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.regular)
                .disabled(mIsEnrolling)
            }
            .padding(.horizontal)
            .padding(.bottom)
        }
        .padding()
        .background(Color.white)
        .alert("MFA Authentication Required", isPresented: $mMfaAlertNeed) {
            Button("Open URL") {
                if let url = URL(string: mMfaAlertURL) {
                    openURL(url)
                }
            }
            Button("Cancel", role: .cancel) { }
        } message: {
            Text("Multi-factor authentication is required to complete enrollment.")
        }
        .alert("Enrollment successful", isPresented: $mShowEnrollmentSuccessAlert) {
            Button("Learn More") {
                if mIsPublicBand {
                    mLearnMoreURL = "https://mud.band/docs/public-band"
                } else {
                    mLearnMoreURL = "https://mud.band/docs/private-band"
                }
                mShowLearnMoreSheet = true
                mShowEnrollmentSuccessAlert = false
            }
            Button("Okay") {
                mShowSuccessToast = true
                DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
                    mAppModel.update_enrollments()
                    dismiss()
                }
            }
        } message: {
            if mIsPublicBand {
                Text("NOTE: This band is public. This means that:\n• Nobody can connect to your device without your permission.\n• Your default policy is 'block'.\n• You need to add an ACL rule to allow the connection.\n• To control ACL, you need to open the WebCLI.")
                    .multilineTextAlignment(.leading)
            } else {
                Text("NOTE: This band is private. This means that:\n• Band admin only can control ACL rules and the default policy.\n• You can't control your device.")
                    .multilineTextAlignment(.leading)
            }
        }
        .sheet(isPresented: $mShowLearnMoreSheet) {
            VStack(spacing: 20) {
                Text("Documentation")
                    .font(.title)
                    .fontWeight(.bold)
                
                Text("You'll be redirected to the documentation website.")
                    .multilineTextAlignment(.center)
                
                HStack(spacing: 20) {
                    Button("Cancel") {
                        mShowLearnMoreSheet = false
                        mShowEnrollmentSuccessAlert = true
                    }
                    .buttonStyle(.bordered)
                    .controlSize(.large)
                    
                    Button("Open Link") {
                        if let url = URL(string: mLearnMoreURL) {
                            openURL(url)
                        }
                        mShowLearnMoreSheet = false
                        mShowEnrollmentSuccessAlert = true
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.large)
                }
            }
            .padding(30)
            .frame(width: 400)
        }
        .toast(isPresenting: $mShowSuccessToast) {
            AlertToast(displayMode: .banner(.slide),
                       type: .complete(.green),
                       title: "Enrollment Successful",
                       subTitle: "Your device has been successfully enrolled.")
        }
        .toast(isPresenting: $mShowErrorToast) {
            AlertToast(displayMode: .hud,
                       type: .error(.red),
                       title: "Enrollment Error",
                       subTitle: mErrorAlertMessage)
        }
    }
}

#Preview {
    UiEnrollmentNewView()
        .frame(minWidth: 400, maxWidth: 500, minHeight: 500, maxHeight: 600)
        .background(Color.white)
}
