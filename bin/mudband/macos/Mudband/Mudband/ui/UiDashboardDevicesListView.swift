//
//  UiDashboardDevicesTabView.swift
//  Mud.band
//
//  Created by Weongyo Jeong on 1/29/25.
//

import Foundation
import SwiftUI
import SwiftyJSON

struct UiDashboardDevicesListView: View {
    struct Device: Identifiable {
        var id = UUID()
        var name: String
        var private_ip: String
    }
    @State var devices: [Device] = []
    
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
    
    private func update_device_list() {
        devices.removeAll()
        guard let obj = read_band_conf() else {
            mudband_ui_log(0, "BANDEC_00420: read_band_conf() failed")
            return
        }
        for (_, peer) in obj["peers"] {
            devices.append(Device(name: peer["name"].stringValue,
                                  private_ip: peer["private_ip"].stringValue))
        }
    }
    
    @ViewBuilder
    var body: some View {
        List(devices, id: \.id) { device in
            VStack(alignment: .leading) {
                Text(device.name).fontWeight(/*@START_MENU_TOKEN@*/.bold/*@END_MENU_TOKEN@*/)
                Text("Private IP: \(device.private_ip)")
            }
        }
        .padding()
        .onAppear() {
            update_device_list()
        }
    }
}

#Preview {
    UiDashboardDevicesListView()
}
