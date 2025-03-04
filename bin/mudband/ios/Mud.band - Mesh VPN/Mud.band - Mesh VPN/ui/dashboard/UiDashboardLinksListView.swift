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

struct UiDashboardLinksListView: View {
    @Environment(\.openURL) var openURL
    struct Link: Identifiable {
        var id = UUID()
        var name: String
        var url: String
    }
    @State var links: [Link] = []
    @State private var searchText: String = ""
    
    private func read_band_conf() -> JSON? {
        guard let band_uuid = mudband_ui_enroll_get_band_uuid() else {
            mudband_ui_log(0, "BANDEC_00534: Can't get the default band UUID.")
            return nil
        }
        guard let enroll_dir = FileManager.EnrollDirURL?.path else {
            mudband_ui_log(0, "BANDEC_00535: Failed to get the enroll dir.")
            return nil
        }
        let filepath = enroll_dir + "/conf_\(band_uuid).json"
        guard let str = try? String(contentsOfFile: filepath, encoding: String.Encoding.utf8) else {
            mudband_ui_log(0, "BANDEC_00536: Failed to parse the band config: \(filepath)")
            return nil
        }
        return JSON(parseJSON: str)
    }
    
    private func update_link_list() {
        links.removeAll()
        guard let obj = read_band_conf() else {
            mudband_ui_log(0, "BANDEC_00537: read_band_conf() failed")
            return
        }
        for (_, link) in obj["links"] {
            links.append(Link(name: link["name"].stringValue,
                              url: link["url"].stringValue))
        }
    }
    
    private var filteredLinks: [Link] {
        if searchText.isEmpty {
            return links
        } else {
            return links.filter { 
                $0.name.localizedCaseInsensitiveContains(searchText) ||
                $0.url.localizedCaseInsensitiveContains(searchText)
            }
        }
    }
    
    @ViewBuilder
    var body: some View {
        VStack {
            if links.isEmpty {
                VStack(spacing: 20) {
                    Image(systemName: "link.circle")
                        .font(.system(size: 60))
                        .foregroundColor(.gray)
                    Text("No links found")
                        .font(.headline)
                    Text("Links added to your band will appear here")
                        .font(.subheadline)
                        .foregroundColor(.gray)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(Color(.systemGroupedBackground))
            } else {
                List {
                    ForEach(filteredLinks) { link in
                        Button(action: {
                            if let url = URL(string: link.url) {
                                openURL(url)
                            }
                        }) {
                            HStack {
                                VStack(alignment: .leading, spacing: 5) {
                                    Text(link.name)
                                        .font(.headline)
                                    Text(link.url)
                                        .font(.subheadline)
                                        .foregroundColor(.gray)
                                        .lineLimit(1)
                                }
                                Spacer()
                                Image(systemName: "arrow.up.right.square")
                                    .font(.system(size: 16))
                                    .foregroundColor(.blue)
                            }
                            .padding(.vertical, 4)
                        }
                        .buttonStyle(PlainButtonStyle())
                    }
                }
                .listStyle(InsetGroupedListStyle())
                .searchable(text: $searchText, prompt: "Search links")
                .overlay(
                    Group {
                        if filteredLinks.isEmpty {
                            VStack(spacing: 15) {
                                Image(systemName: "magnifyingglass")
                                    .font(.system(size: 40))
                                    .foregroundColor(.gray)
                                Text("No matching links found")
                                    .font(.headline)
                            }
                        }
                    }
                )
            }
        }
        .navigationTitle("Links")
        .onAppear() {
            update_link_list()
        }
    }
}

#Preview {
    UiDashboardLinksListView()
}
