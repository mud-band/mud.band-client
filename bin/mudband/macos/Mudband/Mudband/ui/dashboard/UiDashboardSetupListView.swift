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
import Foundation
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
            mudband_ui_log(0, "BANDEC_00421: mudband_ui_enroll_get_band_uuid() failed.")
            unenrollCompletionHandler(band_ui_error.no_band_uuid_found)
            return
        }
        var headers: HTTPHeaders = []
        if let jwt = mudband_ui_enroll_get_jwt() {
            headers["Authorization"] = jwt
        } else {
            mudband_ui_log(0, "BANDEC_00422: mudband_tunnel_enroll_get_jwt() failed.")
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
                        mudband_ui_log(0, "BANDEC_00423: No status field in the response.")
                        unenrollCompletionHandler(band_ui_error.response_body_error)
                        return
                    }
                    if status != 200 {
                        if status == 505 /* No band found */ || status == 506 /* No device found */ {
                            /* do nothing and fallthrough to force the unenrollment. */
                        } else {
                            mudband_ui_log(0, "BANDEC_00424: Failed to unenroll: status \(status)")
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
                    mudband_ui_log(0, "BANDEC_00425: Failed to set the status code.")
                    unenrollCompletionHandler(band_ui_error.response_status_error)
                    return
                }
                mudband_ui_log(0, "BANDEC_00426: \(statusCode) \(error)")
                unenrollCompletionHandler(error)
                break
            }
        }
    }
}

struct UiDashboardSetupEnrollmentNewView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @State private var mCanEnroll = true
    
    private func isEnrollable() -> Bool {
        if mAppModel.mVpnManager.getVPNStatusString() == "Disconnected" ||
           mAppModel.mVpnManager.getVPNStatusString() == "Not_ready" {
            return true
        }
        return false
    }
    
    var body: some View {
        NavigationLink(destination: UiEnrollmentNewView()) {
            HStack(spacing: 12) {
                Image(systemName: "plus.circle.fill")
                    .foregroundColor(.blue)
                    .font(.system(size: 18))
                    .frame(width: 24, height: 24)
                
                VStack(alignment: .leading, spacing: 2) {
                    Text("Add New Enrollment")
                        .font(.system(size: 14, weight: .medium))
                    
                    Text("Set up a new connection")
                        .font(.system(size: 12))
                        .foregroundColor(.secondary)
                }
                
                Spacer()
            }
            .padding(.vertical, 6)
            .contentShape(Rectangle())
        }
        .buttonStyle(PlainButtonStyle())
        .disabled(!mCanEnroll)
        .onAppear {
            mCanEnroll = isEnrollable()
        }
    }
}

struct UiDashboardSetupEnrollmentChangeView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @State var mCanChangeEnrollment = true
    
    func isChangable() -> Bool {
        if mAppModel.mVpnManager.getVPNStatusString() == "Not_ready" ||
           mAppModel.mVpnManager.getVPNStatusString() == "Disconnected" {
            return true
        }
        return false
    }
    
    var body: some View {
        NavigationLink(destination: UiEnrollmentChangeView()) {
            HStack(spacing: 12) {
                Image(systemName: "arrow.triangle.swap")
                    .foregroundColor(.blue)
                    .font(.system(size: 18))
                    .frame(width: 24, height: 24)
                
                VStack(alignment: .leading, spacing: 2) {
                    Text("Change Enrollment")
                        .font(.system(size: 14, weight: .medium))
                    
                    Text("Switch to another connection")
                        .font(.system(size: 12))
                        .foregroundColor(.secondary)
                }
                
                Spacer()
            }
            .padding(.vertical, 6)
            .contentShape(Rectangle())
        }
        .buttonStyle(PlainButtonStyle())
        .disabled(!mCanChangeEnrollment)
        .onAppear {
            mCanChangeEnrollment = isChangable()
        }
    }
}

struct UiDashboardSetupDangerZoneUnenrollView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @ObservedObject private var mUnenrollModel = UiDashboardSetupDangerZoneUnenrollModel()
    @State private var mShowConfirmationSheet = false
    @State private var mUnenrollAlertNeed = false
    @State private var mUnenrollAlertMessage = ""
    @State private var mCanUnenroll = true
    
    var onUnenrollSuccess: () -> Void
    
    private func isUnenrollable() -> Bool {
        if mAppModel.mVpnManager.getVPNStatusString() == "Disconnected" ||
           mAppModel.mVpnManager.getVPNStatusString() == "Not_ready" {
            return true
        }
        return false
    }
    
    var body: some View {
        Button(action: {
            mShowConfirmationSheet = true
        }) {
            HStack(spacing: 12) {
                Image(systemName: "trash.fill")
                    .foregroundColor(.red)
                    .font(.system(size: 18))
                    .frame(width: 24, height: 24)
                
                VStack(alignment: .leading, spacing: 2) {
                    Text("Remove Enrollment")
                        .font(.system(size: 14, weight: .medium))
                        .foregroundColor(.red)
                    
                    Text("Disconnect permanently from this network")
                        .font(.system(size: 12))
                        .foregroundColor(.secondary)
                }
                
                Spacer()
            }
            .padding(.vertical, 6)
            .contentShape(Rectangle())
        }
        .buttonStyle(PlainButtonStyle())
        .disabled(!mCanUnenroll)
        .onAppear {
            mCanUnenroll = isUnenrollable()
        }
        .sheet(isPresented: $mShowConfirmationSheet) {
            VStack(spacing: 20) {
                Image(systemName: "exclamationmark.triangle")
                    .font(.system(size: 36))
                    .foregroundColor(.red)
                    .padding(.top, 24)
                
                Text("Confirm Unenrollment")
                    .font(.headline)
                
                Text("Are you sure you want to remove this enrollment?\nThis action cannot be undone.")
                    .multilineTextAlignment(.center)
                    .frame(maxWidth: 300)
                    .padding(.horizontal)
                
                HStack(spacing: 16) {
                    Button("Cancel") {
                        mShowConfirmationSheet = false
                    }
                    .keyboardShortcut(.escape, modifiers: [])
                    
                    Button("Remove") {
                        mShowConfirmationSheet = false
                        mUnenrollModel.mudband_unenroll(unenrollCompletionHandler: { error in
                            if let error = error {
                                mUnenrollAlertNeed = true
                                mUnenrollAlertMessage = "\(error)"
                                return
                            }
                            onUnenrollSuccess()
                            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                                mAppModel.update_enrollments()
                            }
                        })
                    }
                    .keyboardShortcut(.return, modifiers: [])
                    .foregroundColor(.red)
                }
                .padding(.bottom, 24)
            }
            .frame(width: 400, height: 280)
        }
        .alert(isPresented: $mUnenrollAlertNeed) {
            Alert(
                title: Text("Unenroll Error"),
                message: Text(mUnenrollAlertMessage),
                dismissButton: .default(Text("OK"))
            )
        }
    }
}

struct UiDashboardSetupListView: View {
    @State private var mShowSuccessToast = false
    @Environment(\.colorScheme) var colorScheme
    @EnvironmentObject private var mAppModel: AppModel
    
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header with toolbar look
            HStack {
                Text("Setup")
                    .font(.system(size: 22, weight: .semibold))
            }
            .padding(.horizontal)
            .padding(.top, 16)
            .padding(.bottom, 12)
            
            Divider()
                .padding(.bottom, 16)
            
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    // Enrollment Options Group
                    GroupBox(label: 
                        Label("Enrollment", systemImage: "network")
                            .font(.system(size: 13, weight: .semibold))
                    ) {
                        VStack(spacing: 0) {
                            UiDashboardSetupEnrollmentNewView()
                                .padding(.vertical, 4)
                            
                            Divider()
                                .padding(.leading, 36)
                            
                            UiDashboardSetupEnrollmentChangeView()
                                .padding(.vertical, 4)
                        }
                        .padding(.vertical, 4)
                        .padding(.horizontal, 2)
                    }
                    .groupBoxStyle(MacGroupBoxStyle())
                    
                    // Danger Zone Group
                    GroupBox(label: 
                        Label("Danger Zone", systemImage: "exclamationmark.triangle")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundColor(.red)
                    ) {
                        UiDashboardSetupDangerZoneUnenrollView(onUnenrollSuccess: {
                            mShowSuccessToast = true
                        })
                        .padding(.vertical, 4)
                        .padding(.horizontal, 2)
                    }
                    .groupBoxStyle(DangerZoneGroupBoxStyle())
                }
                .padding(.horizontal)
                .padding(.bottom, 16)
            }
        }
        .background(colorScheme == .dark ? Color(NSColor.windowBackgroundColor) : Color(NSColor.controlBackgroundColor))
        .toast(isPresenting: $mShowSuccessToast) {
            AlertToast(displayMode: .hud, type: .complete(.green), title: "Unenroll Successful")
        }
    }
}

// Custom GroupBox styles for macOS
struct MacGroupBoxStyle: GroupBoxStyle {
    func makeBody(configuration: Configuration) -> some View {
        VStack(alignment: .leading) {
            configuration.label
                .padding(.bottom, 4)
            
            configuration.content
                .padding(10)
                .background(Color(NSColor.controlBackgroundColor))
                .clipShape(RoundedRectangle(cornerRadius: 6))
                .overlay(
                    RoundedRectangle(cornerRadius: 6)
                        .stroke(Color.gray.opacity(0.2), lineWidth: 1)
                )
        }
    }
}

struct DangerZoneGroupBoxStyle: GroupBoxStyle {
    func makeBody(configuration: Configuration) -> some View {
        VStack(alignment: .leading) {
            configuration.label
                .padding(.bottom, 4)
            
            configuration.content
                .padding(10)
                .background(Color.red.opacity(0.05))
                .clipShape(RoundedRectangle(cornerRadius: 6))
                .overlay(
                    RoundedRectangle(cornerRadius: 6)
                        .stroke(Color.red.opacity(0.3), lineWidth: 1)
                )
        }
    }
}
