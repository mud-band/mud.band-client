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

struct UiDashboardListView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @Environment(\.openURL) var openURL
    @State private var selectedItem: String? = "Status"
        
    @State private var items_private = [
        "Status",
        "Devices",
        "Setup",
    ]

    @State private var items_public = [
        "Status",
        "Devices",
        "WebCLI",
        "Setup",
    ]
    
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
            if mAppModel.mBandIsPublic {
                List(selection: $selectedItem) {
                    ForEach(items_public, id: \.self) { folder in
                        NavigationLink(value: folder) {
                            Text(verbatim: folder)
                        }
                    }
                }
                .navigationTitle("Sidebar")
            } else {
                List(selection: $selectedItem) {
                    ForEach(items_private, id: \.self) { folder in
                        NavigationLink(value: folder) {
                            Text(verbatim: folder)
                        }
                    }
                }
                .navigationTitle("Sidebar")
            }
        } detail: {
            if let selectedItem {
                if selectedItem == "Status" {
                    UiDashboardStatusListView()
                        .tag("Status")
                } else if selectedItem == "Devices" {
                    UiDashboardDevicesListView()
                        .tag("Devices")
                } else if selectedItem == "Setup" {
                    UiDashboardSetupListView()
                        .tag("Setup")
                } else if selectedItem == "WebCLI" {
                    Button("Open WebCLI") {
                        getWebCliUrl()
                    }
                } else {
                    NavigationLink(value: selectedItem) {
                        Text(verbatim: selectedItem)
                            .navigationTitle(selectedItem)
                    }
                }
            } else {
                Text("Choose a menu from the sidebar")
            }
        }
    }
}
