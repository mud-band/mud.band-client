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
    var mTopView: UiDashboardView
    @State private var selectedItem: String? = "Status"
        
    @State private var items = [
        "Status",
        "Devices",
        "Setup",
    ]
        
    var body: some View {
        NavigationSplitView {
            List(selection: $selectedItem) {
                ForEach(items, id: \.self) { folder in
                    NavigationLink(value: folder) {
                        Text(verbatim: folder)
                    }
                }
            }
            .navigationTitle("Sidebar")
        } detail: {
            if let selectedItem {
                if selectedItem == "Status" {
                    UiDashboardStatusListView()
                        .tag("Status")
                } else if selectedItem == "Devices" {
                    UiDashboardDevicesListView()
                        .tag("Devices")
                } else if selectedItem == "Setup" {
                    UiDashboardSetupListView(mTopView: mTopView)
                        .tag("Setup")
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
