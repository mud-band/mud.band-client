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

class UiDashboardMainModel : ObservableObject {
    private var mBandName = ""
    private var mTarget: UiDashboardStatusTabView?
    
    func on_appear(target: UiDashboardStatusTabView) {
        self.mTarget = target
        let r = mudband_ui_enroll_load()
        if r != 0 {
            mudband_ui_log(0, "BANDEC_00416: mudband_ui_enroll_load() failed")
            return
        }
        target.mBandNameSelected = mudband_ui_enroll_get_band_name()
    }
    
    func on_disappear(target: UiDashboardStatusTabView) {
        /* do nothing */
    }
        
    func vpn_connect(target: UiDashboardStatusTabView, app_model: AppModel) {
        app_model.mVpnManager.connectVPN()
    }
    
    func vpn_disconnect(target: UiDashboardStatusTabView, app_model: AppModel) {
        app_model.mVpnManager.disconnectVPN()
    }
}

struct UiDashboardStatusTabView: View {
    @ObservedObject private var model = UiDashboardMainModel()
    @EnvironmentObject private var mAppModel: AppModel
    @State var mBandNameSelected: String = ""
    @State var mVPNStatusString = "Unknown"
    let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    @ViewBuilder
    var body: some View {
        VStack {
            Image(systemName: "globe")
                .imageScale(.large)
                .foregroundStyle(.tint)
            HStack {
                Text("Band name:")
                Text(mBandNameSelected)
            }
            if mVPNStatusString == "Connected" {
                Button("Disconnect") {
                    model.vpn_disconnect(target: self, app_model: mAppModel)
                }
            } else if mVPNStatusString == "Disconnected" || mVPNStatusString == "Not_ready" {
                Button("Connect") {
                    model.vpn_connect(target: self, app_model: mAppModel)
                }
            } else {
                Button(action: {
                    
                }) {
                    Text(mVPNStatusString)
                }.disabled(true)
            }
        }
        .padding()
        .onAppear {
            model.on_appear(target: self)
        }
        .onDisappear {
            model.on_disappear(target: self)
        }.onReceive(timer) { _ in
            mVPNStatusString = mAppModel.mVpnManager.getVPNStatusString()
        }
    }
}

struct UiDashboardDevicesTabView: View {
    struct Device: Identifiable {
        var id = UUID()
        var name: String
        var private_ip: String
    }
    @State var devices: [Device] = []
    
    private func read_band_conf() -> JSON? {
        guard let band_uuid = mudband_ui_enroll_get_band_uuid() else {
            mudband_ui_log(0, "BANDEC_00417: Can't get the default band UUID.")
            return nil
        }
        guard let enroll_dir = FileManager.EnrollDirURL?.path else {
            mudband_ui_log(0, "BANDEC_00418: Failed to get the enroll dir.")
            return nil
        }
        let filepath = enroll_dir + "/conf_\(band_uuid).json"
        guard let str = try? String(contentsOfFile: filepath, encoding: String.Encoding.utf8) else {
            mudband_ui_log(0, "BANDEC_00419: Failed to parse the band config: \(filepath)")
            return nil
        }
        return JSON(parseJSON: str)
    }
    
    private func update_device_list() {
        devices.removeAll()
        guard let obj = read_band_conf() else {
            mudband_ui_log(0, "BANDEC_00420: read_band_conf() failed")
            return
        }
        for (_, peer) in obj["peers"] {
            devices.append(Device(name: peer["name"].stringValue,
                                  private_ip: peer["private_ip"].stringValue))
        }
    }
    
    @ViewBuilder
    var body: some View {
        List(devices, id: \.id) { device in
            VStack(alignment: .leading) {
                Text(device.name).fontWeight(/*@START_MENU_TOKEN@*/.bold/*@END_MENU_TOKEN@*/)
                Text("Private IP: \(device.private_ip)")
            }
        }
        .padding()
        .onAppear() {
            update_device_list()
        }
    }
}

class UiDashboardSetupDangerZoneUnenrollModel : ObservableObject {
    enum band_ui_error: Error {
        case no_jwt_token_found
        case no_band_uuid_found
        case response_status_error
        case response_body_error
        case response_body_status_error
    }
    
    func mudband_unenroll(fetchCompletionHandler: @escaping (Error?) -> Void) {
        guard let band_uuid = mudband_ui_enroll_get_band_uuid() else {
            mudband_ui_log(0, "BANDEC_00421: mudband_ui_enroll_get_band_uuid() failed.")
            fetchCompletionHandler(band_ui_error.no_band_uuid_found)
            return
        }
        var headers: HTTPHeaders = []
        if let jwt = mudband_ui_enroll_get_jwt() {
            headers["Authorization"] = jwt
        } else {
            mudband_ui_log(0, "BANDEC_00422: mudband_tunnel_enroll_get_jwt() failed.")
            fetchCompletionHandler(band_ui_error.no_jwt_token_found)
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
                        fetchCompletionHandler(band_ui_error.response_body_error)
                        return
                    }
                    if status != 200 {
                        mudband_ui_log(0, "BANDEC_00424: Failed to unenroll: status \(status)")
                        fetchCompletionHandler(band_ui_error.response_body_status_error)
                        return
                    }
                }
                mudband_ui_enroll_unenroll(band_uuid)
                fetchCompletionHandler(nil)
                break
            case .failure(let error):
                guard let statusCode = resp.response?.statusCode else {
                    mudband_ui_log(0, "BANDEC_00425: Failed to set the status code.")
                    fetchCompletionHandler(band_ui_error.response_status_error)
                    return
                }
                mudband_ui_log(0, "BANDEC_00426: \(statusCode) \(error)")
                fetchCompletionHandler(error)
                break
            }
        }
    }
}

struct UiDashboardSetupDangerZoneUnenrollView: View {
    var mTopView: UiDashboardView
    @Binding var mTabSelection: String
    @EnvironmentObject private var mAppModel: AppModel
    @ObservedObject private var model = UiDashboardSetupDangerZoneUnenrollModel()
    @State private var mNeedToShowPopupConnect = false
    @State private var mUnenrollAlertNeed = false
    @State private var mUnenrollAlertMessage = ""
    @State private var mCanUnenroll = true

    private func need_enrollment_count_refresh() {
        DispatchQueue.main.async {
            mTopView.mNeedEnrollmentCountRefresh = true
        }
    }
    
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
                    model.mudband_unenroll(fetchCompletionHandler: { error in
                        if let error = error {
                            mUnenrollAlertNeed = true
                            mUnenrollAlertMessage = "\(error)"
                            return
                        }
                        mNeedToShowPopupConnect = false
                        mTabSelection = "Status"
                        need_enrollment_count_refresh()
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

struct UiDashboardSetupTabView: View {
    var mTopView: UiDashboardView
    @Binding var mTabSelection: String

    @ViewBuilder
    var body: some View {
        VStack {
            List {
                Section(header: Text("Enrollment")) {
                    UiDashboardSetupEnrollmentNewView()
                    UiDashboardSetupEnrollmentChangeView()
                }
                Section(header: Text("DANGER ZONE")) {
                    UiDashboardSetupDangerZoneUnenrollView(mTopView: mTopView, mTabSelection: $mTabSelection)
                }
            }.padding()
        }
    }
}

struct UiDashboardTabView: View {
    var mTopView: UiDashboardView
    @State private var mTabSelection = "Status"
    
    @ViewBuilder
    var body: some View {
        NavigationStack {
            VStack {
                TabView(selection: $mTabSelection) {
                    UiDashboardStatusTabView()
                        .tabItem {
                            Label("Status", systemImage: "list.dash")
                        }
                        .tag("Status")
                    UiDashboardDevicesTabView()
                        .tabItem {
                            Label("Devices", systemImage: "square.and.pencil")
                        }
                        .tag("Devices")
                    UiDashboardSetupTabView(mTopView: mTopView, mTabSelection: $mTabSelection)
                        .tabItem {
                            Label("Setup", systemImage: "square.and.pencil")
                        }
                        .tag("Setup")
                }
            }
            .padding()
            .navigationDestination(for: String.self) { value in
                if value == "EnrollmentNew" {
                    UiEnrollmentNewView()
                } else if value == "EnrollmentChange" {
                    UiEnrollmentChangeView()
                }
            }
        }
    }
}

struct UiDashboardView: View {
    @State var mEnrollmentCount: Int = Int(mudband_ui_enroll_get_count())
    @State var mNeedEnrollmentCountRefresh = false
    let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    @ViewBuilder
    var body: some View {
        VStack {
            if mEnrollmentCount > 0 {
                UiDashboardTabView(mTopView: self)
            } else {
                NavigationStack {
                    VStack {
                        Image(systemName: "globe")
                            .imageScale(.large)
                            .foregroundStyle(.tint)
                        HStack {
                            Text("No enrollment found.  Please enroll first.")
                        }
                        NavigationLink("Enroll", destination: UiEnrollmentNewView())
                    }
                }
            }
        }.onReceive(timer) { _ in
            if mEnrollmentCount == 0 || mNeedEnrollmentCountRefresh {
                mEnrollmentCount = Int(mudband_ui_enroll_get_count())
                mNeedEnrollmentCountRefresh = false
            }
        }
    }
}

#Preview {
    UiDashboardView()
}
