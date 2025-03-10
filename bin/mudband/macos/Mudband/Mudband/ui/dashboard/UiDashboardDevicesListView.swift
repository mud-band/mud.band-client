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
import SwiftyJSON

struct UiDashboardDevicesListView: View {
    struct Device: Identifiable {
        var id = UUID()
        var name: String
        var private_ip: String
        var endpoint_t_heartbeated: String
    }
    @State var devices: [Device] = []
    @State private var searchText: String = ""
    @State private var isRefreshing: Bool = false
    
    private var filteredDevices: [Device] {
        if searchText.isEmpty {
            return devices
        } else {
            return devices.filter { 
                $0.name.localizedCaseInsensitiveContains(searchText) ||
                $0.private_ip.localizedCaseInsensitiveContains(searchText)
            }
        }
    }
    
    private func read_status_snapshot() -> JSON? {
        guard let top_dir = FileManager.TopDirURL?.path else {
            mudband_ui_log(0, "BANDEC_00888: Failed to get the top dir.")
            return nil
        }
        let filepath = top_dir + "/status_snapshot.json"
        guard let str = try? String(contentsOfFile: filepath, encoding: String.Encoding.utf8) else {
            mudband_ui_log(0, "BANDEC_00889: Failed to parse the band config: \(filepath)")
            return nil
        }
        return JSON(parseJSON: str)
    }
    
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
    
    private func formatRelativeTime(timestamp: Int64) -> String {
        if timestamp == 0 {
            return "Never"
        }
        
        let now = Int64(Date().timeIntervalSince1970)
        let diff = now - timestamp
        
        if diff < 0 {
            return "Future"
        } else if diff < 60 {
            return "\(diff)s ago"
        } else if diff < 3600 {
            return "\(diff / 60)m ago"
        } else if diff < 86400 {
            return "\(diff / 3600)h ago"
        } else if diff < 2592000 {
            return "\(diff / 86400)d ago"
        } else if diff < 31536000 {
            return "\(diff / 2592000)mo ago"
        } else {
            return "\(diff / 31536000)y ago"
        }
    }
    
    private func update_device_list() {
        devices.removeAll()
        guard let confObj = read_band_conf() else {
            mudband_ui_log(0, "BANDEC_00420: read_band_conf() failed")
            return
        }
        let statusSnapshotObj = read_status_snapshot()
        for (_, peer) in confObj["peers"] {
            var endpoint_t_heartbeated: Int64 = 0
            if statusSnapshotObj != nil {
                for (_, statusPeer) in statusSnapshotObj!["peers"] {
                    if statusPeer["iface_addr"].stringValue == peer["private_ip"].stringValue {
                        endpoint_t_heartbeated = statusPeer["endpoint_t_heartbeated"].int64Value
                        break
                    }
                }
            }
            devices.append(Device(name: peer["name"].stringValue,
                                  private_ip: peer["private_ip"].stringValue,
                                  endpoint_t_heartbeated: formatRelativeTime(timestamp: endpoint_t_heartbeated)))
        }
    }
    
    @ViewBuilder
    var body: some View {
        VStack(spacing: 0) {
            // Search bar
            HStack {
                Image(systemName: "magnifyingglass")
                    .foregroundColor(.gray)
                TextField("Search devices...", text: $searchText)
                    .padding(.vertical, 10)
                
                if !searchText.isEmpty {
                    Button(action: {
                        searchText = ""
                    }) {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundColor(.gray)
                    }
                }
            }
            .cornerRadius(10)
            .padding()
            
            if isRefreshing {
                ProgressView()
                    .padding()
            }
            
            if filteredDevices.isEmpty {
                VStack {
                    Image(systemName: "network")
                        .font(.system(size: 50))
                        .foregroundColor(.gray)
                        .padding()
                    
                    if searchText.isEmpty {
                        Text("No devices found.")
                            .font(.headline)
                        Text("Pull down to refresh the list")
                            .font(.subheadline)
                            .foregroundColor(.gray)
                    } else {
                        Text("No matching devices")
                            .font(.headline)
                        Text("Try a different search term")
                            .font(.subheadline)
                            .foregroundColor(.gray)
                    }
                }
                .padding()
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .center)
            } else {
                List {
                    ForEach(filteredDevices) { device in
                        HStack(spacing: 15) {
                            ZStack {
                                Circle()
                                    .fill(Color.blue.opacity(0.1))
                                    .frame(width: 40, height: 40)
                                
                                Image(systemName: "laptopcomputer")
                                    .font(.system(size: 18))
                                    .foregroundColor(.blue)
                            }
                            
                            VStack(alignment: .leading, spacing: 5) {
                                Text(device.name)
                                    .font(.headline)
                                
                                HStack(spacing: 10) {
                                    HStack(spacing: 4) {
                                        Image(systemName: "network")
                                            .font(.caption)
                                            .foregroundColor(.gray)
                                        Text(device.private_ip)
                                            .font(.subheadline)
                                            .foregroundColor(.secondary)
                                    }
                                    
                                    HStack(spacing: 4) {
                                        Image(systemName: "heart.fill")
                                            .font(.caption)
                                            .foregroundColor(.gray)
                                        Text(device.endpoint_t_heartbeated)
                                            .font(.subheadline)
                                            .foregroundColor(.secondary)
                                    }
                                }
                            }
                        }
                        .padding(.vertical, 8)
                    }
                }
                .padding()
                .refreshable {
                    isRefreshing = true
                    update_device_list()
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                        isRefreshing = false
                    }
                }
            }
        }
        .onAppear() {
            update_device_list()
        }
    }
}

#Preview {
    UiDashboardDevicesListView()
}
