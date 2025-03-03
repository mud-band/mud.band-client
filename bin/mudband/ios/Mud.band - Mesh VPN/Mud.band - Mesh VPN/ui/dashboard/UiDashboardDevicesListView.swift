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

struct UiDashboardDevicesListView: View {
    struct Device: Identifiable {
        var id = UUID()
        var name: String
        var private_ip: String
    }
    @State var devices: [Device] = []
    @State private var searchText: String = ""
    
    private func read_band_conf() -> JSON? {
        guard let band_uuid = mudband_ui_enroll_get_band_uuid() else {
            mudband_ui_log(0, "BANDEC_00280: Can't get the default band UUID.")
            return nil
        }
        guard let enroll_dir = FileManager.EnrollDirURL?.path else {
            mudband_ui_log(0, "BANDEC_00281: Failed to get the enroll dir.")
            return nil
        }
        let filepath = enroll_dir + "/conf_\(band_uuid).json"
        guard let str = try? String(contentsOfFile: filepath, encoding: String.Encoding.utf8) else {
            mudband_ui_log(0, "BANDEC_00282: Failed to parse the band config: \(filepath)")
            return nil
        }
        return JSON(parseJSON: str)
    }
    
    private func update_device_list() {
        devices.removeAll()
        guard let obj = read_band_conf() else {
            mudband_ui_log(0, "BANDEC_00283: read_band_conf() failed")
            return
        }
        for (_, peer) in obj["peers"] {
            devices.append(Device(name: peer["name"].stringValue,
                                  private_ip: peer["private_ip"].stringValue))
        }
    }
    
    private var filteredDevices: [Device] {
        if searchText.isEmpty {
            return devices
        } else {
            return devices.filter { device in
                device.name.localizedCaseInsensitiveContains(searchText) ||
                device.private_ip.localizedCaseInsensitiveContains(searchText)
            }
        }
    }
    
    @ViewBuilder
    var body: some View {
        VStack {
            if devices.isEmpty {
                VStack(spacing: 20) {
                    Image(systemName: "desktopcomputer")
                        .font(.system(size: 60))
                        .foregroundColor(.gray)
                    Text("No devices found")
                        .font(.headline)
                    Text("Devices connected to your band will appear here")
                        .font(.subheadline)
                        .foregroundColor(.gray)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(Color(.systemGroupedBackground))
            } else {
                List {
                    ForEach(filteredDevices) { device in
                        HStack {
                            VStack(alignment: .leading, spacing: 5) {
                                Text(device.name)
                                    .font(.headline)
                                HStack {
                                    Image(systemName: "network")
                                        .font(.caption)
                                        .foregroundColor(.gray)
                                    Text(device.private_ip)
                                        .font(.subheadline)
                                        .foregroundColor(.gray)
                                }
                            }
                            Spacer()
                            Image(systemName: "desktopcomputer")
                                .font(.system(size: 16))
                                .foregroundColor(.blue)
                        }
                        .padding(.vertical, 4)
                    }
                }
                .listStyle(InsetGroupedListStyle())
                .searchable(text: $searchText, prompt: "Search devices")
                .overlay(
                    Group {
                        if filteredDevices.isEmpty {
                            VStack(spacing: 15) {
                                Image(systemName: "magnifyingglass")
                                    .font(.system(size: 40))
                                    .foregroundColor(.gray)
                                Text("No matching devices found")
                                    .font(.headline)
                            }
                        }
                    }
                )
            }
        }
        .navigationTitle("Devices")
        .onAppear() {
            update_device_list()
        }
    }
}
