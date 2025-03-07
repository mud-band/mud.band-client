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
    @Environment(\.colorScheme) var colorScheme
    
    struct Link: Identifiable {
        var id = UUID()
        var name: String
        var url: String
    }
    @State var links: [Link] = []
    @State private var isRefreshing = false
    @State private var searchText = ""
    
    private var filteredLinks: [Link] {
        if searchText.isEmpty {
            return links
        } else {
            return links.filter { link in
                link.name.localizedCaseInsensitiveContains(searchText) ||
                link.url.localizedCaseInsensitiveContains(searchText)
            }
        }
    }
    
    private func read_band_conf() -> JSON? {
        guard let band_uuid = mudband_ui_enroll_get_band_uuid() else {
            mudband_ui_log(0, "BANDEC_00545: Can't get the default band UUID.")
            return nil
        }
        guard let enroll_dir = FileManager.EnrollDirURL?.path else {
            mudband_ui_log(0, "BANDEC_00546: Failed to get the enroll dir.")
            return nil
        }
        let filepath = enroll_dir + "/conf_\(band_uuid).json"
        guard let str = try? String(contentsOfFile: filepath, encoding: String.Encoding.utf8) else {
            mudband_ui_log(0, "BANDEC_00547: Failed to parse the band config: \(filepath)")
            return nil
        }
        return JSON(parseJSON: str)
    }
    
    private func update_link_list() {
        links.removeAll()
        guard let obj = read_band_conf() else {
            mudband_ui_log(0, "BANDEC_00548: read_band_conf() failed")
            return
        }
        for (_, link) in obj["links"] {
            links.append(Link(name: link["name"].stringValue,
                              url: link["url"].stringValue))
        }
    }
    
    @ViewBuilder
    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Image(systemName: "magnifyingglass")
                    .foregroundColor(.secondary)
                
                TextField("Search links...", text: $searchText)
                    .disableAutocorrection(true)
                
                if !searchText.isEmpty {
                    Button(action: {
                        searchText = ""
                    }) {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundColor(.secondary)
                    }
                    .buttonStyle(PlainButtonStyle())
                    .transition(.opacity)
                    .animation(.easeInOut(duration: 0.2), value: searchText)
                }
            }
            .padding(8)
            .padding(.horizontal)
            .padding(.vertical, 8)
            
            if links.isEmpty {
                VStack(spacing: 20) {
                    Image(systemName: "link.circle")
                        .font(.system(size: 60))
                        .foregroundColor(.secondary)
                    
                    Text("No links found")
                        .font(.headline)
                        .foregroundColor(.secondary)                    
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .padding()
            } else if filteredLinks.isEmpty {
                VStack(spacing: 20) {
                    Image(systemName: "magnifyingglass")
                        .font(.system(size: 60))
                        .foregroundColor(.secondary)
                    
                    Text("No matching links")
                        .font(.headline)
                        .foregroundColor(.secondary)
                    
                    Text("Try a different search term")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .padding()
            } else {
                ScrollView {
                    LazyVStack(spacing: 10) {
                        ForEach(filteredLinks) { link in
                            MacLinkCardView(link: link, openURL: openURL)
                                .transition(.opacity)
                        }
                    }
                    .padding(.horizontal)
                    .padding(.vertical, 8)
                }
            }
        }
        .onAppear() {
            update_link_list()
        }
    }
}

struct MacLinkCardView: View {
    let link: UiDashboardLinksListView.Link
    let openURL: OpenURLAction
    @State private var isHovered = false
    @Environment(\.colorScheme) var colorScheme
    
    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 6) {
                Text(link.name)
                    .font(.headline)
                    .fontWeight(.medium)
                
                Text(link.url)
                    .font(.system(size: 12))
                    .foregroundColor(.secondary)
                    .lineLimit(1)
            }
            
            Spacer()
            
            Image(systemName: "arrow.up.right")
                .font(.system(size: 14))
                .foregroundColor(.accentColor)
        }
        .padding(10)
        .background(
            RoundedRectangle(cornerRadius: 8)
                .fill(isHovered ? 
                      (colorScheme == .dark ? Color(NSColor.darkGray).opacity(0.3) : Color(NSColor.lightGray).opacity(0.2)) : 
                      (colorScheme == .dark ? Color(NSColor.windowBackgroundColor) : Color(NSColor.controlBackgroundColor)))
                .overlay(
                    RoundedRectangle(cornerRadius: 8)
                        .stroke(Color.gray.opacity(0.2), lineWidth: 1)
                )
        )
        .contentShape(Rectangle())
        .onHover { hovering in
            withAnimation(.easeInOut(duration: 0.2)) {
                isHovered = hovering
            }
        }
        .onTapGesture {
            if let url = URL(string: link.url) {
                openURL(url)
            }
        }
        .help("Open \(link.url)")
        .animation(.easeInOut(duration: 0.2), value: isHovered)
        .contextMenu {
            Button("Open Link") {
                if let url = URL(string: link.url) {
                    openURL(url)
                }
            }
            
            Button("Copy URL") {
                let pasteboard = NSPasteboard.general
                pasteboard.clearContents()
                pasteboard.setString(link.url, forType: .string)
            }
        }
    }
}

#Preview {
    UiDashboardLinksListView()
}
