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
import SwiftUI
import SwiftyJSON

class UiEnrollmentChangeModel : ObservableObject {
}

struct UiEnrollmentChangeView: View {
    @ObservedObject private var model = UiEnrollmentChangeModel()
    @Environment(\.dismiss) var dismiss
    struct Enrollment: Identifiable {
        var id = UUID()
        var name: String
        var band_uuid: String
    }
    @State var enrollments: [Enrollment] = []

    private func read_band_json(band_uuid: String) -> JSON? {
        guard let enroll_dir = FileManager.EnrollDirURL?.path else {
            mudband_ui_log(0, "BANDEC_00273: Failed to get the enroll dir.")
            return nil
        }
        let filepath = enroll_dir + "/band_\(band_uuid).json"
        guard let str = try? String(contentsOfFile: filepath, encoding: String.Encoding.utf8) else {
            mudband_ui_log(0, "BANDEC_00274: Failed to parse the band config: \(filepath)")
            return nil
        }
        return JSON(parseJSON: str)
    }
    
    private func update_enrollment_list() {
        enrollments.removeAll()
        guard let band_uuids = mudband_ui_enroll_get_band_uuids() as? [String] else {
            mudband_ui_log(0, "BANDEC_00275: mudband_ui_enroll_get_band_uuids() failed.")
            return
        }
        for band_uuid in band_uuids {
            guard let obj = read_band_json(band_uuid: band_uuid) else {
                continue
            }
            enrollments.append(Enrollment(name: obj["name"].stringValue,
                                          band_uuid: band_uuid))
        }
    }
    
    private func change_enrollment_default_band_uuid(band_uuid: String) {
        mudband_ui_progconf_set_default_band_uuid_objc(band_uuid)
        self.dismiss()
    }
    
    @ViewBuilder
    var body: some View {
        NavigationStack {
            VStack {
                List(enrollments, id: \.id) { enrollment in
                    HStack{
                        Text(enrollment.name)
                        Spacer()
                        Button {
                            change_enrollment_default_band_uuid(band_uuid: enrollment.band_uuid)
                        } label: {
                            Text("Change")
                        }
                    }
                }
                .onAppear() {
                    update_enrollment_list()
                }
            }
            .navigationTitle("Enrollment List")
        }
    }
}

#Preview {
    UiEnrollmentChangeView()
}
