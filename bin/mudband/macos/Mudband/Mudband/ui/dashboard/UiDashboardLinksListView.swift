//
//  UiDashboardDevicesTabView.swift
//  Mud.band
//
//  Created by Weongyo Jeong on 1/29/25.
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
    
    private func read_band_conf() -> JSON? {
        guard let band_uuid = mudband_ui_enroll_get_band_uuid() else {
            mudband_ui_log(0, "BANDEC_XXXXX: Can't get the default band UUID.")
            return nil
        }
        guard let enroll_dir = FileManager.EnrollDirURL?.path else {
            mudband_ui_log(0, "BANDEC_XXXXX: Failed to get the enroll dir.")
            return nil
        }
        let filepath = enroll_dir + "/conf_\(band_uuid).json"
        guard let str = try? String(contentsOfFile: filepath, encoding: String.Encoding.utf8) else {
            mudband_ui_log(0, "BANDEC_XXXXX: Failed to parse the band config: \(filepath)")
            return nil
        }
        return JSON(parseJSON: str)
    }
    
    private func update_link_list() {
        links.removeAll()
        guard let obj = read_band_conf() else {
            mudband_ui_log(0, "BANDEC_XXXXX: read_band_conf() failed")
            return
        }
        for (_, link) in obj["links"] {
            links.append(Link(name: link["name"].stringValue,
                              url: link["url"].stringValue))
        }
    }
    
    @ViewBuilder
    var body: some View {
        VStack {
            if links.isEmpty {
                Text("No links found.")
            } else {
                List(links, id: \.id) { link in
                    VStack(alignment: .leading) {
                        Text(link.name).fontWeight(.bold)
                        Text("URL: \(link.url)")
                    }
                    .onTapGesture {
                        if let url = URL(string: link.url) {
                            openURL(url)
                        }
                    }
                }
                .padding()
            }
        }
        .onAppear() {
            update_link_list()
        }
    }
}

#Preview {
    UiDashboardLinksListView()
}
