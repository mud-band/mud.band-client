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

struct UiBandCreateAsGuestView: View {
    @EnvironmentObject private var mAppModel: AppModel
    
    @State private var bandName: String = ""
    @State private var bandDescription: String = ""
    @State private var isSubmitting: Bool = false
    @State private var nameFieldIsFocused: Bool = false
    @State private var descriptionFieldIsFocused: Bool = false
    @State private var progressMessage: String = "Creating your band..."
    @Environment(\.presentationMode) var presentationMode
    
    @State private var showToast = false
    @State private var toastMessage = ""
    @State private var toastType: AlertToast.AlertType = .error(.red)
    
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

    private func enroll_set_result(status: Int, msg: String) {
        DispatchQueue.main.async {
            isSubmitting = false
            if status != 200 {
                toastMessage = msg
                toastType = .error(.red)
                showToast = true
            }
        }
    }

    func enroll_success(priv_key: String, raw_str: String) {
        let r = mudband_ui_enroll_post(priv_key, raw_str)
        if r != 0 {
            mudband_ui_log(0, "BANDEC_XXXXX: mudband_ui_enroll() failed.")
            return
        }
        DispatchQueue.main.async {
            toastMessage = "Band creation and enrollment successfully completed!"
            toastType = .complete(.green)
            showToast = true
            
            // Update enrollments and dismiss after a short delay to show the toast
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                mAppModel.update_enrollments()
                self.presentationMode.wrappedValue.dismiss()
            }
        }
    }
    
    func enroll(enrollment_token: String, device_name: String) async {
        let keys = mudband_ui_create_wireguard_keys()
        if keys == nil || keys?.count != 2 {
            enroll_set_result(status: 500,
                              msg: "BANDEC_XXXXX: mudband_ui_create_wireguard_keys() failed.")
            return
        }
        let pub_key: String = keys![0] as! String
        let priv_key: String = keys![1] as! String
        let parameters = enroll_params(token: enrollment_token,
                                       name: device_name,
                                       wireguard_pubkey: pub_key,
                                       secret: "")
        let resp = await AF.request("https://www.mud.band/api/band/enroll",
                                    method: .post,
                                    parameters: parameters,
                                    encoder: JSONParameterEncoder.default,
                                    interceptor: .retryPolicy)
            .serializingString().response
        switch resp.result {
        case .success(let str):
            if let obj = try? JSON(data: Data(str.utf8)) {
                if obj["status"].intValue != 200 {
                    if obj["status"].intValue == 301 {
                        enroll_set_result(status: 301,
			                  msg: obj["sso_url"].stringValue)
                        return
                    }
                    enroll_set_result(status: 501,
                                      msg: obj["msg"].stringValue)
                    return
                }
            }
            enroll_success(priv_key: priv_key, raw_str: str)
        case .failure(let error):
            enroll_set_result(status: 502,
                              msg: "\(error)")
        }
        enroll_set_result(status: 200, msg: "")
    }

    var body: some View {
        ZStack {
            Color(NSColor.windowBackgroundColor).edgesIgnoringSafeArea(.all)
            
            VStack(spacing: 0) {
                // Header
                VStack(spacing: 6) {
                    Text("Create Band as Guest")
                        .font(.title3)
                        .fontWeight(.bold)
                    
                    Text("Enter details for your new band")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .padding(.top, 20)
                .padding(.bottom, 16)
                .frame(maxWidth: .infinity)
                
                VStack(alignment: .leading, spacing: 16) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Band Name")
                            .fontWeight(.medium)
                        
                        TextField("Enter a name for your band", text: $bandName)
                            .textFieldStyle(PlainTextFieldStyle())
                            .padding(8)
                            .background(
                                RoundedRectangle(cornerRadius: 8)
                                    .stroke(nameFieldIsFocused ? Color.blue : Color.gray.opacity(0.3), lineWidth: nameFieldIsFocused ? 2 : 1)
                                    .background(Color.gray.opacity(0.05).cornerRadius(8))
                            )
                            .onTapGesture {
                                nameFieldIsFocused = true
                                descriptionFieldIsFocused = false
                            }
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Band Description")
                            .fontWeight(.medium)
                        
                        ZStack(alignment: .topLeading) {
                            if bandDescription.isEmpty {
                                Text("Describe what your band is about")
                                    .foregroundColor(.gray.opacity(0.7))
                                    .padding(.horizontal, 10)
                                    .padding(.vertical, 10)
                                    .allowsHitTesting(false)
                            }
                            
                            TextEditor(text: $bandDescription)
                                .font(.body)
                                .scrollContentBackground(.hidden)
                                .background(Color.clear)
                                .padding(6)
                                .onTapGesture {
                                    descriptionFieldIsFocused = true
                                    nameFieldIsFocused = false
                                }
                        }
                        .frame(height: 100)
                        .background(
                            RoundedRectangle(cornerRadius: 8)
                                .stroke(descriptionFieldIsFocused ? Color.blue : Color.gray.opacity(0.3), lineWidth: descriptionFieldIsFocused ? 2 : 1)
                                .background(Color.gray.opacity(0.05).cornerRadius(8))
                        )
                    }
                    
                    VStack(alignment: .leading, spacing: 4) {
                        Text("The default band type is private and the default ACL policy is open.")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                            .padding(10)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
                .padding(.horizontal, 20)
                
                Spacer()
                
                HStack(spacing: 12) {
                    Button(action: createBand) {
                        HStack(spacing: 8) {
                            if isSubmitting {
                                ProgressView()
                                    .controlSize(.small)
                                    .scaleEffect(0.7)
                            }
                            
                            Text(isSubmitting ? "Creating..." : "Create Band")
                        }
                        .frame(minWidth: 100)
                        .padding(.horizontal, 12)
                        .padding(.vertical, 8)
                        .background(bandName.isEmpty ? Color.blue.opacity(0.5) : Color.blue)
                        .foregroundColor(.white)
                        .cornerRadius(8)
                    }
                    .buttonStyle(PlainButtonStyle())
                    .disabled(isSubmitting)
                    .keyboardShortcut(.defaultAction)
                }
                .padding(.bottom, 20)
                .padding(.top, 16)
            }
            .background(Color.white)
            
            if isSubmitting {
                ZStack {
                    Color.black.opacity(0.4)
                        .edgesIgnoringSafeArea(.all)
                    
                    VStack(spacing: 16) {
                        ProgressView()
                            .scaleEffect(1.5)
                            .padding()
                        
                        Text(progressMessage)
                            .font(.headline)
                    }
                    .padding(24)
                    .background(
                        RoundedRectangle(cornerRadius: 12)
                            .fill(Color(.windowBackgroundColor).opacity(0.9))
                    )
                    .shadow(radius: 10)
                }
                .transition(.opacity)
            }
        }
        .animation(.easeInOut(duration: 0.2), value: isSubmitting)
        .toast(isPresenting: $showToast) {
            AlertToast(
                displayMode: .hud,
                type: toastType,
                title: toastMessage
            )
        }
    }
    
    private func createBand() {
        isSubmitting = true
        progressMessage = "Creating your band..."
        
        Task {
            let parameters: [String: Any] = [
                "name": bandName,
                "description": bandDescription
            ]
            let request = AF.request("https://www.mud.band/api/band/anonymous/create",
                                     method: .post,
                                     parameters: parameters,
                                     encoding: JSONEncoding.default)
            let response = await request.serializingData().response
            
            if let data = response.data {
                let json = JSON(data)
                if json["status"].intValue == 200 {
                    if let jwtToken = json["jwt"].string {
                        if let bandUuid = json["band_uuid"].string {
                            mudband_ui_bandadmin_save(bandUuid, jwtToken)
                            Task {
                                await createEnrollmentToken(jwtToken: jwtToken)
                            }
                        } else {
                            isSubmitting = false
                            toastMessage = "Band UUID missing from response"
                            toastType = .error(.red)
                            showToast = true
                        }
                    } else {
                        isSubmitting = false
                        toastMessage = "JWT token missing from response"
                        toastType = .error(.red)
                        showToast = true
                    }
                } else {
                    isSubmitting = false
                    toastMessage = json["msg"].stringValue
                    toastType = .error(.red)
                    showToast = true
                }
            } else if let error = response.error {
                isSubmitting = false
                toastMessage = "Network error: \(error.localizedDescription)"
                toastType = .error(.red)
                showToast = true
            }
        }
    }
    
    private func createEnrollmentToken(jwtToken: String) async {
        DispatchQueue.main.async {
            progressMessage = "Creating enrollment token..."
        }
        
        let headers: HTTPHeaders = [
            "Authorization": jwtToken
        ]
        
        let request = AF.request("https://www.mud.band/api/band/anonymous/enrollment/token/create",
                                 method: .get,
                                 headers: headers)
        
        let response = await request.serializingData().response
        
        if let data = response.data {
            let json = JSON(data)
            if json["status"].intValue == 200 {
                if let enrollmentToken = json["token"].string {
                    DispatchQueue.main.async {
                        progressMessage = "Enrolling device..."
                    }
                    let deviceName = generateDeviceName()
                    await enroll(enrollment_token: enrollmentToken, device_name: deviceName)
                } else {
                    DispatchQueue.main.async {
                        isSubmitting = false
                        toastMessage = "Enrollment token missing from response"
                        toastType = .error(.red)
                        showToast = true
                    }
                }
            } else {
                DispatchQueue.main.async {
                    isSubmitting = false
                    toastMessage = "Failed to create enrollment token: \(json["msg"].stringValue)"
                    toastType = .error(.red)
                    showToast = true
                }
            }
        } else if let error = response.error {
            DispatchQueue.main.async {
                isSubmitting = false
                toastMessage = "Network error creating enrollment token: \(error.localizedDescription)"
                toastType = .error(.red)
                showToast = true
            }
        }
    }
    
    private func generateDeviceName() -> String {
        let hostname = ProcessInfo.processInfo.hostName
        let randomSuffix = generateRandomAlphanumeric(length: 6)
        return "\(hostname)-\(randomSuffix)"
    }
    
    private func generateRandomAlphanumeric(length: Int) -> String {
        let characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
        return String((0..<length).map { _ in characters.randomElement()! })
    }
}


