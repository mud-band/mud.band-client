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
import NetworkExtension
import SwiftUI
import SwiftyJSON

class UiDashboardSetupDangerZoneUnenrollModel : ObservableObject {
    enum band_ui_error: Error {
        case no_jwt_token_found
        case no_band_uuid_found
        case response_status_error
        case response_body_error
        case response_body_status_error
    }
    
    func mudband_unenroll(unenrollCompletionHandler: @escaping (Error?) -> Void) {
        guard let band_uuid = mudband_ui_enroll_get_band_uuid() else {
            mudband_ui_log(0, "BANDEC_00284: mudband_ui_enroll_get_band_uuid() failed.")
            unenrollCompletionHandler(band_ui_error.no_band_uuid_found)
            return
        }
        var headers: HTTPHeaders = []
        if let jwt = mudband_ui_enroll_get_jwt() {
            headers["Authorization"] = jwt
        } else {
            mudband_ui_log(0, "BANDEC_00285: mudband_tunnel_enroll_get_jwt() failed.")
            unenrollCompletionHandler(band_ui_error.no_jwt_token_found)
            return
        }
        AF.request("https://www.mud.band/api/band/unenroll",
                   method: .get,
                   headers: headers,
                   interceptor: .retryPolicy)
        .responseString { resp in
            switch resp.result {
            case .success(let body):
                if let obj = try? JSON(data: Data(body.utf8)) {
                    guard let status = obj["status"].int else {
                        mudband_ui_log(0, "BANDEC_00286: No status field in the response.")
                        unenrollCompletionHandler(band_ui_error.response_body_error)
                        return
                    }
                    if status != 200 {
                        if status == 505 /* No band found */ || status == 506 /* No device found */ {
                            /* do nothing and fallthrough to force the unenrollment. */
                        } else {
                            mudband_ui_log(0, "BANDEC_00287: Failed to unenroll: status \(status)")
                            unenrollCompletionHandler(band_ui_error.response_body_status_error)
                            return
                        }
                    }
                }
                mudband_ui_enroll_unenroll(band_uuid)
                unenrollCompletionHandler(nil)
                break
            case .failure(let error):
                guard let statusCode = resp.response?.statusCode else {
                    mudband_ui_log(0, "BANDEC_00288: Failed to set the status code.")
                    unenrollCompletionHandler(band_ui_error.response_status_error)
                    return
                }
                mudband_ui_log(0, "BANDEC_00289: \(statusCode) \(error)")
                unenrollCompletionHandler(error)
                break
            }
        }
    }
}

struct UiDashboardSetupDangerZoneUnenrollView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @Environment(\.dismiss) var dismiss
    @ObservedObject private var mUnenrollModel = UiDashboardSetupDangerZoneUnenrollModel()
    @State private var mNeedToShowPopupConnect = false
    @State private var mUnenrollAlertNeed = false
    @State private var mUnenrollAlertMessage = ""
    @State private var mCanUnenroll = true
    @State private var mIsUnenrolling = false
    
    private func isUnenrollable() -> Bool {
        if mAppModel.mVpnManager.getVPNStatusString() == "Disconnected" ||
            mAppModel.mVpnManager.getVPNStatusString() == "Not_ready" {
            return true
        }
        return false
    }
    
    var body: some View {
        VStack(alignment: .leading) {
            HStack(spacing: 12) {
                Image(systemName: "xmark.circle.fill")
                    .foregroundColor(.red)
                    .font(.title2)
                
                VStack(alignment: .leading, spacing: 4) {
                    Text("Remove Enrollment")
                        .font(.headline)
                    
                    Text("Disconnect and remove this device from the network")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                Button(action: {
                    mNeedToShowPopupConnect = true
                }) {
                    Text("Unenroll")
                        .fontWeight(.medium)
                        .padding(.horizontal, 12)
                        .padding(.vertical, 8)
                        .background(mCanUnenroll ? Color.red.opacity(0.8) : Color.gray.opacity(0.3))
                        .foregroundColor(.white)
                        .cornerRadius(8)
                }
                .disabled(!mCanUnenroll)
            }
            .padding()
            .background(Color(UIColor.secondarySystemBackground))
            .cornerRadius(12)
        }
        .onAppear {
            mCanUnenroll = isUnenrollable()
        }
        .popover(isPresented: $mNeedToShowPopupConnect,
                 attachmentAnchor: .point(.bottom),
                 arrowEdge: .bottom) {
            VStack(spacing: 20) {
                Image(systemName: "exclamationmark.triangle.fill")
                    .foregroundColor(.orange)
                    .font(.system(size: 40))
                    .padding(.top)
                
                Text("Do you really want to remove the enrollment?")
                    .font(.headline)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal)
                
                Text("This action cannot be undone.")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
                    .multilineTextAlignment(.center)
                
                HStack(spacing: 20) {
                    Button(action: {
                        mNeedToShowPopupConnect = false
                    }) {
                        Text("Cancel")
                            .fontWeight(.medium)
                            .frame(minWidth: 100)
                            .padding(.vertical, 8)
                            .foregroundColor(.primary)
                            .background(Color(UIColor.secondarySystemBackground))
                            .cornerRadius(8)
                    }
                    
                    Button(action: {
                        mIsUnenrolling = true
                        mUnenrollModel.mudband_unenroll(unenrollCompletionHandler: { error in
                            mIsUnenrolling = false
                            mNeedToShowPopupConnect = false
                            if let error = error {
                                mUnenrollAlertNeed = true
                                mUnenrollAlertMessage = "\(error)"
                                return
                            }                            
                            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                                mAppModel.update_enrollments()
                            }
                        })
                    }) {
                        let backgroundColor = mIsUnenrolling ? Color.gray.opacity(0.7) : Color.red
                        let textColor = mIsUnenrolling ? Color.gray.opacity(0.9) : Color.white
                        HStack {
                            Text("Yes")
                                .fontWeight(.medium)
                                .foregroundColor(textColor)
                            
                            if mIsUnenrolling {
                                ProgressView()
                                    .progressViewStyle(CircularProgressViewStyle(tint: textColor))
                                    .scaleEffect(0.8)
                            }
                        }
                        .frame(minWidth: 100)
                        .padding(.vertical, 8)
                        .background(backgroundColor)
                        .cornerRadius(8)
                    }
                    .disabled(mIsUnenrolling)
                }
                .padding(.bottom)
            }
            .frame(width: 300)
            .padding()
        }
        .alert("Unenroll Error", isPresented: $mUnenrollAlertNeed) {
            Button("OK", role: .cancel) { }
        } message: {
            Text(mUnenrollAlertMessage)
        }
    }
}

struct UiDashboardSetupEnrollmentChangeView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @State var mCanChangeEnrollment = true
    @State private var navigateToChangeView = false
    
    private func isChangable() -> Bool {
        if mAppModel.mVpnManager.getVPNStatusString() == "Disconnected" ||
            mAppModel.mVpnManager.getVPNStatusString() == "Not_ready" {
            return true
        }
        return false
    }
    
    var body: some View {
        Button(action: {
            if mCanChangeEnrollment {
                navigateToChangeView = true
            }
        }) {
            HStack(spacing: 12) {
                Image(systemName: "arrow.triangle.swap")
                    .foregroundColor(.blue)
                    .font(.title2)
                
                VStack(alignment: .leading, spacing: 4) {
                    Text("Change Enrollment")
                        .font(.headline)
                    
                    Text("Switch to a different band")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                Image(systemName: "chevron.right")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            .padding()
            .background(Color(UIColor.secondarySystemBackground))
            .cornerRadius(12)
        }
        .buttonStyle(PlainButtonStyle())
        .disabled(!mCanChangeEnrollment)
        .opacity(mCanChangeEnrollment ? 1.0 : 0.6)
        .onAppear {
            mCanChangeEnrollment = isChangable()
        }
        .navigationDestination(isPresented: $navigateToChangeView) {
            UiEnrollmentChangeView()
        }
    }
}

struct UiDashboardSetupEnrollmentNewView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @State private var mCanEnroll = true
    @State private var navigateToNewView = false

    private func isEnrollable() -> Bool {
        if mAppModel.mVpnManager.getVPNStatusString() == "Disconnected" ||
            mAppModel.mVpnManager.getVPNStatusString() == "Not_ready" {
            return true
        }
        return false
    }
    
    var body: some View {
        Button(action: {
            if mCanEnroll {
                navigateToNewView = true
            }
        }) {
            HStack(spacing: 12) {
                Image(systemName: "plus.circle.fill")
                    .foregroundColor(.green)
                    .font(.title2)
                
                VStack(alignment: .leading, spacing: 4) {
                    Text("Add New Enrollment")
                        .font(.headline)
                    
                    Text("Connect to a new network")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                Image(systemName: "chevron.right")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            .padding()
            .background(Color(UIColor.secondarySystemBackground))
            .cornerRadius(12)
        }
        .buttonStyle(PlainButtonStyle())
        .disabled(!mCanEnroll)
        .opacity(mCanEnroll ? 1.0 : 0.6)
        .onAppear {
            mCanEnroll = isEnrollable()
        }
        .navigationDestination(isPresented: $navigateToNewView) {
            UiEnrollmentNewView()
        }
    }
}

struct UiBandCreateAsGuestOptionView: View {
    @State private var navigateToBandCreateView = false
    
    var body: some View {
        Button(action: {
            navigateToBandCreateView = true
        }) {
            HStack(spacing: 12) {
                Image(systemName: "network")
                    .foregroundColor(.purple)
                    .font(.title2)
                
                VStack(alignment: .leading, spacing: 4) {
                    Text("Create Band as Guest")
                        .font(.headline)
                    
                    Text("Create a new band network")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                
                Spacer()
                
                Image(systemName: "chevron.right")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            .padding()
            .background(Color(UIColor.secondarySystemBackground))
            .cornerRadius(12)
        }
        .buttonStyle(PlainButtonStyle())
        .navigationDestination(isPresented: $navigateToBandCreateView) {
            UiBandCreateAsGuestView()
        }
    }
}

struct UiDashboardSetupListView: View {
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 24) {
                    VStack(alignment: .leading, spacing: 16) {
                        Text("BAND")
                            .font(.caption)
                            .fontWeight(.semibold)
                            .foregroundColor(.secondary)
                            .padding(.horizontal)
                        
                        UiBandCreateAsGuestOptionView()
                    }
                    
                    VStack(alignment: .leading, spacing: 16) {
                        Text("ENROLLMENT OPTIONS")
                            .font(.caption)
                            .fontWeight(.semibold)
                            .foregroundColor(.secondary)
                            .padding(.horizontal)
                        
                        UiDashboardSetupEnrollmentNewView()
                        UiDashboardSetupEnrollmentChangeView()
                    }
                    
                    VStack(alignment: .leading, spacing: 16) {
                        Text("DANGER ZONE")
                            .font(.caption)
                            .fontWeight(.semibold)
                            .foregroundColor(.red)
                            .padding(.horizontal)
                        
                        UiDashboardSetupDangerZoneUnenrollView()
                    }
                }
                .padding()
            }
            .background(Color(UIColor.systemGroupedBackground))
            .navigationTitle("Settings")
        }
    }
}
