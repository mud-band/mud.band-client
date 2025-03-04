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
    @Environment(\.colorScheme) var colorScheme
    @State private var selectedItem: String? = "Status"
    
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
    
    // 메뉴 아이템에 사용할 아이콘을 반환하는 함수
    func iconForItem(_ item: String) -> String {
        switch item {
        case "Status":
            return "gauge"
        case "Devices":
            return "desktopcomputer"
        case "Links":
            return "link"
        case "WebCLI":
            return "terminal"
        case "Setup":
            return "gearshape"
        default:
            return "circle"
        }
    }
    
    func getWebCliUrl() {
        var headers: HTTPHeaders = []
        if let jwt = mudband_ui_enroll_get_jwt() {
            headers["Authorization"] = jwt
        } else {
            mudband_ui_log(0, "BANDEC_00528: mudband_ui_enroll_get_jwt() failed.")
            return
        }
        AF.request("https://www.mud.band/webcli/signin",
                   method: .get,
                   headers: headers,
                   interceptor: .retryPolicy).responseString { resp in
            switch resp.result {
            case .success(let resp_body):
                guard let obj = try? JSON(data: Data(resp_body.utf8)) else {
                    mudband_ui_log(0, "BANDEC_00529: Failed to parse the JSON response.")
                    return
                }
                if obj["status"].intValue != 200 {
                    let msg = obj["msg"].stringValue
                    mudband_ui_log(0, "BANDEC_00530: Failed with the error message: \(msg)")
                    return
                }
                if let urlString = obj["url"].string, let url = URL(string: urlString) {
                    openURL(url)
                } else {
                    mudband_ui_log(0, "BANDEC_00531: Failed to retrieve or open the SSO URL.")
                }
            case .failure(let error):
                guard let statusCode = resp.response?.statusCode else {
                    mudband_ui_log(0, "BANDEC_00532: Failed to set the status code.")
                    return
                }
                mudband_ui_log(0, "BANDEC_00533: \(statusCode) \(error)")
                return
            }
        }
    }
    
    var body: some View {
        NavigationSplitView {
            List(selection: $selectedItem) {
                ForEach(mAppModel.mBandIsPublic ? items_public : items_private, id: \.self) { item in
                    Label {
                        Text(verbatim: item)
                    } icon: {
                        Image(systemName: iconForItem(item))
                    }
                    .padding(.vertical, 4)
                }
            }
            .navigationTitle("Mud.band")
            .listStyle(SidebarListStyle())
            .onAppear {
                if selectedItem == nil {
                    selectedItem = "Status"
                }
            }
        } detail: {
            if let selectedItem {
                VStack {
                    switch selectedItem {
                    case "Status":
                        UiDashboardStatusListView()
                            .tag("Status")
                            .transition(.opacity)
                    case "Devices":
                        UiDashboardDevicesListView()
                            .tag("Devices")
                            .transition(.opacity)
                    case "Links":
                        UiDashboardLinksListView()
                            .tag("Links")
                            .transition(.opacity)
                    case "Setup":
                        UiDashboardSetupListView()
                            .tag("Setup")
                            .transition(.opacity)
                    case "WebCLI":
                        Spacer()
                        VStack(spacing: 20) {
                            Image(systemName: "terminal")
                                .font(.system(size: 60))
                                .foregroundColor(.accentColor)
                            
                            Text("Access Web Command Line Interface")
                                .font(.headline)
                            
                            Button(action: getWebCliUrl) {
                                HStack {
                                    Image(systemName: "globe")
                                    Text("Open WebCLI")
                                }
                                .padding()
                                .frame(minWidth: 200)
                                .background(Color.accentColor)
                                .foregroundColor(.white)
                                .cornerRadius(10)
                                .shadow(radius: 2)
                            }
                            .buttonStyle(PlainButtonStyle())
                        }
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                        .background(colorScheme == .dark ? Color.black.opacity(0.3) : Color.white.opacity(0.7))
                        .cornerRadius(12)
                        .padding()
                    default:
                        Text(verbatim: selectedItem)
                            .navigationTitle(selectedItem)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .navigationTitle(selectedItem)
            } else {
                VStack(spacing: 20) {
                    Image(systemName: "sidebar.left")
                        .font(.system(size: 60))
                        .foregroundColor(.gray)
                    Text("Choose a menu from the sidebar")
                        .font(.headline)
                        .foregroundColor(.gray)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .accentColor(.blue)
    }
}
