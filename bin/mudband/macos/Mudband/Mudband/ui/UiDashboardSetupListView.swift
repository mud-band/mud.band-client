//
//  UiDashboardSetupTabView.swift
//  Mud.band
//
//  Created by Weongyo Jeong on 1/29/25.
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
                        mudband_ui_log(0, "BANDEC_00424: Failed to unenroll: status \(status)")
                        unenrollCompletionHandler(band_ui_error.response_body_status_error)
                        return
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
                    mUnenrollModel.mudband_unenroll(unenrollCompletionHandler: { error in
                        if let error = error {
                            mUnenrollAlertNeed = true
                            mUnenrollAlertMessage = "\(error)"
                            return
                        }
                        mNeedToShowPopupConnect = false
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
