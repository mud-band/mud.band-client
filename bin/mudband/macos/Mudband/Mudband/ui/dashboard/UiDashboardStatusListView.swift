//
//  UiDashboardStatusTabView.swift
//  Mud.band
//
//  Created by Weongyo Jeong on 1/29/25.
//

import Foundation
import SwiftUI

struct UiDashboardStatusListView: View {
    @EnvironmentObject private var mAppModel: AppModel
    let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    @ViewBuilder
    var body: some View {
        VStack {
            Image(systemName: "globe")
                .imageScale(.large)
                .foregroundStyle(.tint)
            HStack {
                Text("Band name:")
                Text(mAppModel.mBandName)
            }
            if mAppModel.mVPNStatusString == "Connected" {
                Button("Disconnect") {
                    mAppModel.mVpnManager.disconnectVPN()
                }
            } else if mAppModel.mVPNStatusString == "Disconnected" ||
                        mAppModel.mVPNStatusString == "Not_ready" {
                Button("Connect") {
                    mAppModel.mVpnManager.connectVPN()
                }
            } else {
                Button(action: {
                    
                }) {
                    Text(mAppModel.mVPNStatusString)
                }.disabled(true)
            }
        }.onReceive(timer) { _ in
            mAppModel.update_vpn_status()
        }
    }
}
