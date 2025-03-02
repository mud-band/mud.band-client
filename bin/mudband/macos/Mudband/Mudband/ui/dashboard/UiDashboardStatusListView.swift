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

import Foundation
import SwiftUI

struct UiDashboardStatusListView: View {
    @EnvironmentObject private var mAppModel: AppModel
    let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    @ViewBuilder
    var body: some View {
        VStack(spacing: 12) {
            // Header
            VStack(spacing: 8) {
                Image(systemName: "network")
                    .font(.system(size: 32))
                    .foregroundColor(.accentColor)
                    .frame(width: 36, height: 36)
                
                Text("Status")
                    .font(.title3)
                    .fontWeight(.bold)
                    .foregroundColor(.primary)
            }
            .padding(.top, 12)
            
            VStack(spacing: 12) {
                HStack {
                    Label {
                        Text("Connection Details")
                            .font(.headline)
                            .foregroundColor(.primary)
                    } icon: {
                        Image(systemName: "info.circle")
                            .foregroundColor(.accentColor)
                            .frame(width: 18, height: 18)
                    }
                    Spacer()
                }
                .padding(.bottom, 4)
                
                // Connection Details
                VStack(spacing: 8) {
                    HStack {
                        Image(systemName: "antenna.radiowaves.left.and.right")
                            .foregroundColor(.accentColor)
                            .frame(width: 18, height: 18)
                        
                        Text("Band Name:")
                            .foregroundColor(.secondary)
                            .lineLimit(1)
                        
                        Spacer(minLength: 8)
                        
                        Text(mAppModel.mBandName)
                            .fontWeight(.medium)
                            .foregroundColor(.primary)
                            .textSelection(.enabled)
                            .lineLimit(1)
                    }
                    .padding(.vertical, 2)
                    
                    // Device Info
                    if !mAppModel.mDeviceName.isEmpty {
                        HStack {
                            Image(systemName: "desktopcomputer")
                                .foregroundColor(.accentColor)
                                .frame(width: 18, height: 18)
                            
                            Text("Device Name:")
                                .foregroundColor(.secondary)
                                .lineLimit(1)
                            
                            Spacer(minLength: 8)
                            
                            Text(mAppModel.mDeviceName)
                                .fontWeight(.medium)
                                .foregroundColor(.primary)
                                .textSelection(.enabled)
                                .lineLimit(1)
                        }
                        .padding(.vertical, 2)
                    }
                    
                    // IP Address
                    if !mAppModel.mPrivateIP.isEmpty {
                        HStack {
                            Image(systemName: "network")
                                .foregroundColor(.accentColor)
                                .frame(width: 18, height: 18)
                            
                            Text("Private IP:")
                                .foregroundColor(.secondary)
                                .lineLimit(1)
                            
                            Spacer(minLength: 8)
                            
                            Text(mAppModel.mPrivateIP)
                                .fontWeight(.medium)
                                .foregroundColor(.primary)
                                .textSelection(.enabled)
                                .lineLimit(1)
                        }
                        .padding(.vertical, 2)
                    }
                                        
                    // Connection Status
                    HStack {
                        Image(systemName: connectionStatusIcon)
                            .foregroundColor(connectionStatusColor)
                            .frame(width: 18, height: 18)
                        
                        Text("Status:")
                            .foregroundColor(.secondary)
                            .lineLimit(1)
                        
                        Spacer(minLength: 8)
                        
                        Text(mAppModel.mVPNStatusString)
                            .fontWeight(.medium)
                            .foregroundColor(connectionStatusColor)
                            .textSelection(.enabled)
                            .lineLimit(1)
                    }
                    .padding(.vertical, 2)
                }
            }
            .padding(.horizontal)
            .background(Color.white)
            .padding(.horizontal)
            
            // Connection Button
            connectionButton
                .padding(.horizontal)
                .padding(.top, 8)
            
            Spacer()
        }
        .padding(12)
        .background(Color.white)
        .onReceive(timer) { _ in
            mAppModel.update_vpn_status()
        }
    }
    
    // Helper Views
    private var connectionStatusIcon: String {
        if mAppModel.mVPNStatusString == "Connected" {
            return "checkmark.circle.fill"
        } else if mAppModel.mVPNStatusString == "Disconnected" {
            return "x.circle.fill"
        } else {
            return "ellipsis.circle.fill"
        }
    }
    
    private var connectionStatusColor: Color {
        if mAppModel.mVPNStatusString == "Connected" {
            return .green
        } else if mAppModel.mVPNStatusString == "Disconnected" {
            return .red
        } else {
            return .orange
        }
    }
    
    private var connectionButton: some View {
        Group {
            if mAppModel.mVPNStatusString == "Connected" {
                Button(action: {
                    mAppModel.mVpnManager.disconnectVPN()
                }) {
                    HStack {
                        Image(systemName: "xmark.circle.fill")
                            .frame(width: 18, height: 18)
                        Text("Disconnect")
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                }
                .buttonStyle(.borderedProminent)
                .tint(.red)
                .controlSize(.regular)
                .shadow(color: Color.black.opacity(0.1), radius: 2, x: 0, y: 1)
            } else if mAppModel.mVPNStatusString == "Disconnected" || mAppModel.mVPNStatusString == "Not_ready" {
                Button(action: {
                    mAppModel.mVpnManager.connectVPN()
                }) {
                    HStack {
                        Image(systemName: "lock.shield.fill")
                            .frame(width: 18, height: 18)
                        Text("Connect")
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                }
                .buttonStyle(.borderedProminent)
                .tint(.blue)
                .controlSize(.regular)
                .shadow(color: Color.black.opacity(0.1), radius: 2, x: 0, y: 1)
            } else {
                Button(action: {}) {
                    HStack {
                        Image(systemName: "ellipsis.circle.fill")
                            .frame(width: 18, height: 18)
                        Text(mAppModel.mVPNStatusString)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                }
                .controlSize(.regular)
                .disabled(true)
            }
        }
    }
}
