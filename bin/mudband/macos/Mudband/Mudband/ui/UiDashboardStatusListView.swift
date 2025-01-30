//
//  UiDashboardStatusTabView.swift
//  Mud.band
//
//  Created by Weongyo Jeong on 1/29/25.
//

import Foundation
import SwiftUI

class UiDashboardStatusListModal : ObservableObject {
    private var mBandName = ""
    private var mTarget: UiDashboardStatusListView?
    
    func on_appear(target: UiDashboardStatusListView) {
        self.mTarget = target
        let r = mudband_ui_enroll_load()
        if r != 0 {
            mudband_ui_log(0, "BANDEC_00416: mudband_ui_enroll_load() failed")
            return
        }
        target.mBandNameSelected = mudband_ui_enroll_get_band_name()
    }
    
    func on_disappear(target: UiDashboardStatusListView) {
        /* do nothing */
    }
        
    func vpn_connect(target: UiDashboardStatusListView, app_model: AppModel) {
        app_model.mVpnManager.connectVPN()
    }
    
    func vpn_disconnect(target: UiDashboardStatusListView, app_model: AppModel) {
        app_model.mVpnManager.disconnectVPN()
    }
}

struct UiDashboardStatusListView: View {
    @ObservedObject private var model = UiDashboardStatusListModal()
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
