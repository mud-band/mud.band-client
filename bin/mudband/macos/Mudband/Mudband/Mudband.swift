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

import SwiftUI

class AppModel: ObservableObject {
    @Published var mVpnManager: VpnManager;
    @Published var mVPNStatusString = "Unknown"
    @Published var mEnrollmentCount: Int32 = mudband_ui_enroll_get_count()
    @Published var mBandIsPublic: Bool = mudband_ui_enroll_is_public()
    @Published var mBandAdminJsonString: String? = mudband_ui_bandadmin_get()
    @Published var mBandName: String = ""
    @Published var mDeviceName: String = ""
    @Published var mPrivateIP: String = ""
    @Published var mUserTosAgreed: Bool
    
    init() {
        mUserTosAgreed = UserDefaults.standard.bool(forKey: "user_tos_agreed")
        mVpnManager = VpnManager()
        mVpnManager.initVPNTunnelProviderManager()
    }
    
    func sync_user_tos_agreement() {
        mUserTosAgreed = UserDefaults.standard.bool(forKey: "user_tos_agreed")
    }
    
    func set_user_tos_agreement(agreed: Bool) {
        UserDefaults.standard.set(true, forKey: "user_tos_agreed")
        sync_user_tos_agreement()
    }
    
    func update_enrollments() {
        mEnrollmentCount = mudband_ui_enroll_get_count()
        let r = mudband_ui_enroll_load()
        if r != 0 {
            mudband_ui_log(0, "BANDEC_00416: mudband_ui_enroll_load() failed")
            return
        }
        mBandIsPublic = mudband_ui_enroll_is_public()
        mBandName = mudband_ui_enroll_get_band_name()
	mBandAdminJsonString = mudband_ui_bandadmin_get()
        mDeviceName = mudband_ui_confmgr_get_device_name()
        mPrivateIP = mudband_ui_confmgr_get_private_ip()
    }
    
    func update_vpn_status() {
        mVPNStatusString = mVpnManager.getVPNStatusString()
        if mVPNStatusString == "Connected" && mPrivateIP.isEmpty {
            mDeviceName = mudband_ui_confmgr_get_device_name()
            mPrivateIP = mudband_ui_confmgr_get_private_ip()
	    mBandAdminJsonString = mudband_ui_bandadmin_get()
        }
    }
}

class AppDelegate: NSObject, NSApplicationDelegate, ObservableObject {
    override init() {
        FileManager.initMudbandAppGroupDirs()
        mudband_ui_init(FileManager.TopDirURL?.path,
                        FileManager.EnrollDirURL?.path,
                        FileManager.AdminDirURL?.path,
                        FileManager.UiLogFileURL?.path,
                        FileManager.TunnelLogFileURL?.path)
        super.init()
    }
    
    func applicationDidFinishLaunching(_ aNotification: Notification) {
        mudband_ui_log(2, "finished to launch the app.")
    }
}

@main
struct MudbandApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var mAppDelegate
    @StateObject private var model = AppModel()
    
    var body: some Scene {
        WindowGroup {
            UiMainEntryView()
                .environmentObject(model)
                .frame(minWidth: 400, maxWidth: 500, minHeight: 500, maxHeight: 600)
        }.windowResizability(.contentSize)
    }
}
