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

struct UiDashboardSetupDangerZoneUnenrollView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @ObservedObject private var mUnenrollModel = UiDashboardSetupDangerZoneUnenrollModel()
    @State private var mNeedToShowPopupConnect = false
    @State private var mUnenrollAlertNeed = false
    @State private var mUnenrollAlertMessage = ""
    @State private var mCanUnenroll = true
    
    private func isUnenrollable() -> Bool {
        if mAppModel.mVpnManager.getVPNStatusString() == "Disconnected" ||
           mAppModel.mVpnManager.getVPNStatusString() == "Not_ready" {
            return true
        }
        return false
    }
    
    var body: some View {
        HStack {
            Text("Remove the enrollment")
            Spacer()
            Button(action: {
                mNeedToShowPopupConnect = true
            }) {
                Text("Unenroll").foregroundStyle(.red)
            }
            .disabled(mCanUnenroll == false)
        }
        .onAppear {
            if isUnenrollable() {
                mCanUnenroll = true
            } else {
                mCanUnenroll = false
            }
        }
        .popover(isPresented: $mNeedToShowPopupConnect,
                 attachmentAnchor: .point(.bottom),
                 arrowEdge: .bottom) {
            Text("Do you really want to remove the enrollment?")
                .font(.headline)
                .padding()
            HStack {
                Button("Yes") {
                    mNeedToShowPopupConnect = false
                    mUnenrollModel.mudband_unenroll(unenrollCompletionHandler: { error in
                        if let error = error {
                            mUnenrollAlertNeed = true
                            mUnenrollAlertMessage = "\(error)"
                            return
                        }
                        mAppModel.update_enrollments()
                    })
                }
                Button("No") {
                    mNeedToShowPopupConnect = false
                }
            }.padding()
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
    
    func isChangable() -> Bool {
        if mAppModel.mVpnManager.getVPNStatusString() == "Not_ready" ||
           mAppModel.mVpnManager.getVPNStatusString() == "Disconnected" {
            return true
        }
        return false
    }
    
    var body: some View {
        HStack {
            NavigationLink(destination: UiEnrollmentChangeView()) {
                Text("Change the enrollment")
            }
            .disabled(mCanChangeEnrollment == false)
        }
        .onAppear {
            if isChangable() {
                mCanChangeEnrollment = true
            } else {
                mCanChangeEnrollment = false
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
        HStack {
            NavigationLink(destination: UiEnrollmentNewView()) {
                Text("Add new enrollment")
            }
            .disabled(mCanEnroll == false)
        }
        .onAppear {
            if isEnrollable() {
                mCanEnroll = true
            } else {
                mCanEnroll = false
            }
        }
    }
}

struct UiDashboardSetupListView: View {
    @ViewBuilder
    var body: some View {
        VStack {
            List {
                Section(header: Text("Enrollment")) {
                    UiDashboardSetupEnrollmentNewView()
                    UiDashboardSetupEnrollmentChangeView()
                }
                Section(header: Text("DANGER ZONE")) {
                    UiDashboardSetupDangerZoneUnenrollView()
                }
            }.padding()
        }
    }
}
