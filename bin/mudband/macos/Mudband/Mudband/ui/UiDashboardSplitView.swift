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

struct UiDashboardSplitView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @Environment(\.openURL) var openURL
    @State private var selectedItem: String? = "Status"
    @Environment(\.colorScheme) private var colorScheme
        
    @State private var items_private = [
        "Status",
        "Devices",
        "Links",
        "Setup",
    ]

    @State private var items_public = [
        "Status",
        "Devices",
        "Links",
        "WebCLI",
        "Setup",
    ]
    
    // Helper to get icon for menu item
    func getIcon(for item: String) -> String {
        switch item {
        case "Status":
            return "chart.bar"
        case "Devices":
            return "network"
        case "Links":
            return "link"
        case "WebCLI":
            return "terminal"
        case "Setup":
            return "gearshape"
        default:
            return "questionmark"
        }
    }
    
    func getWebCliUrl() {
        var headers: HTTPHeaders = []
        if let jwt = mudband_ui_enroll_get_jwt() {
            headers["Authorization"] = jwt
        } else {
            mudband_ui_log(0, "BANDEC_00515: mudband_ui_enroll_get_jwt() failed.")
            return
        }
        AF.request("https://www.mud.band/webcli/signin",
                   method: .get,
                   headers: headers,
                   interceptor: .retryPolicy).responseString { resp in
            switch resp.result {
            case .success(let resp_body):
                guard let obj = try? JSON(data: Data(resp_body.utf8)) else {
                    mudband_ui_log(0, "BANDEC_00516: Failed to parse the JSON response.")
                    return
                }
                if obj["status"].intValue != 200 {
                    let msg = obj["msg"].stringValue
                    mudband_ui_log(0, "BANDEC_00517: Failed with the error message: \(msg)")
                    return
                }
                if let urlString = obj["url"].string, let url = URL(string: urlString) {
                    openURL(url)
                } else {
                    mudband_ui_log(0, "BANDEC_00518: Failed to retrieve or open the SSO URL.")
                }
            case .failure(let error):
                guard let statusCode = resp.response?.statusCode else {
                    mudband_ui_log(0, "BANDEC_00519: Failed to set the status code.")
                    return
                }
                mudband_ui_log(0, "BANDEC_00520: \(statusCode) \(error)")
                return
            }
        }
    }

    var body: some View {
        NavigationSplitView {
            List(selection: $selectedItem) {
                Section {
                    ForEach(mAppModel.mBandIsPublic ? items_public : items_private, id: \.self) { item in
                        NavigationLink(value: item) {
                            Label {
                                Text(item)
                                    .font(.body)
                            } icon: {
                                Image(systemName: getIcon(for: item))
                            }
                        }
                        .padding(.vertical, 4)
                    }
                }
            }
            .listStyle(.sidebar)
            .navigationTitle("Mud.band")
            .navigationSubtitle("Control Panel")
            .toolbar {
                ToolbarItem(placement: .automatic) {
                    Button(action: {
                        NSApp.keyWindow?.firstResponder?.tryToPerform(#selector(NSSplitViewController.toggleSidebar(_:)), with: nil)
                    }) {
                        Image(systemName: "sidebar.left")
                    }
                }
            }
        } detail: {
            if let selectedItem {
                VStack(alignment: .leading, spacing: 0) {
                    // Header with macOS style
                    ZStack(alignment: .leading) {
                        Rectangle()
                            .fill(Color(NSColor.controlBackgroundColor))
                            .frame(height: 45)
                        
                        HStack(spacing: 12) {
                            Image(systemName: getIcon(for: selectedItem))
                                .font(.system(size: 18, weight: .semibold))
                            
                            Text(selectedItem)
                                .font(.system(size: 18, weight: .semibold))
                                .foregroundColor(Color(NSColor.labelColor))
                        }
                        .padding(.leading)
                    }
                    .frame(maxWidth: .infinity)
                    
                    Divider()
                    
                    // Content area
                    Group {
                        if selectedItem == "Status" {
                            UiDashboardStatusListView()
                                .tag("Status")
                        } else if selectedItem == "Devices" {
                            UiDashboardDevicesListView()
                                .tag("Devices")
                        } else if selectedItem == "Links" {
                            UiDashboardLinksListView()
                                .tag("Links")
                        } else if selectedItem == "Setup" {
                            UiDashboardSetupListView()
                                .tag("Setup")
                        } else if selectedItem == "WebCLI" {
                            VStack(spacing: 24) {
                                Spacer()
                                
                                Image(systemName: "terminal")
                                    .font(.system(size: 56))
                                    .symbolRenderingMode(.hierarchical)
                                    .foregroundStyle(Color.accentColor)
                                
                                Text("Web Command Line Interface")
                                    .font(.title2)
                                    .fontWeight(.medium)
                                
                                Text("Access the web-based command line interface")
                                    .font(.subheadline)
                                    .foregroundColor(Color(NSColor.secondaryLabelColor))
                                    .multilineTextAlignment(.center)
                                    .padding(.horizontal)
                                
                                Button(action: getWebCliUrl) {
                                    Text("Open WebCLI in Browser")
                                }
                                .buttonStyle(.borderedProminent)
                                .controlSize(.large)
                                
                                Spacer()
                            }
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                        } else {
                            Text(verbatim: selectedItem)
                        }
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                }
                .background(Color(NSColor.windowBackgroundColor))
            } else {
                VStack(spacing: 16) {
                    Image(systemName: "arrow.left")
                        .font(.system(size: 36))
                        .foregroundColor(Color(NSColor.secondaryLabelColor))
                    
                    Text("Select an option from the sidebar")
                        .font(.title3)
                        .foregroundColor(Color(NSColor.secondaryLabelColor))
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(Color(NSColor.windowBackgroundColor))
            }
        }
        .toolbar {
            ToolbarItem(placement: .automatic) {
                Button(action: {}) {
                    Image(systemName: "gear")
                }
                .help("Settings")
            }
        }
    }
}
