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

struct UiEnrollmentChangeView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @Environment(\.dismiss) var dismiss
    @State private var searchText = ""
    @State private var selectedEnrollment: String?
    
    struct Enrollment: Identifiable {
        var id = UUID()
        var name: String
        var band_uuid: String
    }
    @State var enrollments: [Enrollment] = []

    private func read_band_json(band_uuid: String) -> JSON? {
        guard let enroll_dir = FileManager.EnrollDirURL?.path else {
            mudband_ui_log(0, "BANDEC_00410: Failed to get the enroll dir.")
            return nil
        }
        let filepath = enroll_dir + "/band_\(band_uuid).json"
        guard let str = try? String(contentsOfFile: filepath, encoding: String.Encoding.utf8) else {
            mudband_ui_log(0, "BANDEC_00411: Failed to parse the band config: \(filepath)")
            return nil
        }
        return JSON(parseJSON: str)
    }
    
    private func update_enrollment_list() {
        enrollments.removeAll()
        guard let band_uuids = mudband_ui_enroll_get_band_uuids() as? [String] else {
            mudband_ui_log(0, "BANDEC_00412: mudband_ui_enroll_get_band_uuids() failed.")
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
        mAppModel.update_enrollments()
        self.dismiss()
    }
    
    private var filteredEnrollments: [Enrollment] {
        if searchText.isEmpty {
            return enrollments
        } else {
            return enrollments.filter { $0.name.localizedCaseInsensitiveContains(searchText) }
        }
    }
    
    @ViewBuilder
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header
            HStack {
                Text("Select Enrollment")
                    .font(.system(size: 18, weight: .semibold))
                
                Spacer()
                
                Button(action: { dismiss() }) {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundColor(.gray)
                        .font(.system(size: 16))
                }
                .buttonStyle(.plain)
            }
            .padding([.horizontal, .top])
            .padding(.bottom, 8)
            
            // Search bar
            HStack {
                Image(systemName: "magnifyingglass")
                    .foregroundColor(.gray)
                
                TextField("Search", text: $searchText)
                    .textFieldStyle(PlainTextFieldStyle())
                
                if !searchText.isEmpty {
                    Button(action: { searchText = "" }) {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundColor(.gray)
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(7)
            .background(Color(white: 0.95))
            .cornerRadius(8)
            .padding(.horizontal)
            .padding(.bottom, 12)
            
            // Divider
            Divider()
                .padding(.horizontal)
            
            // Enrollment list
            if filteredEnrollments.isEmpty {
                VStack(spacing: 12) {
                    Image(systemName: "questionmark.folder")
                        .font(.system(size: 32))
                        .foregroundColor(.gray)
                    Text("No enrollments found")
                        .foregroundColor(.gray)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .padding()
            } else {
                List(selection: $selectedEnrollment) {
                    ForEach(filteredEnrollments) { enrollment in
                        EnrollmentRow(enrollment: enrollment, isSelected: selectedEnrollment == enrollment.band_uuid) {
                            change_enrollment_default_band_uuid(band_uuid: enrollment.band_uuid)
                        }
                        .tag(enrollment.band_uuid)
                    }
                }
                .listStyle(InsetListStyle())
                .background(Color.white)
            }
            
            // Footer
            HStack {
                Spacer()
                Button("Cancel") {
                    dismiss()
                }
                .keyboardShortcut(.cancelAction)
                
                Button("Select") {
                    if let selected = selectedEnrollment,
                       let enrollment = enrollments.first(where: { $0.band_uuid == selected }) {
                        change_enrollment_default_band_uuid(band_uuid: enrollment.band_uuid)
                    }
                }
                .keyboardShortcut(.defaultAction)
                .disabled(selectedEnrollment == nil)
                .buttonStyle(.borderedProminent)
            }
            .padding()
        }
        .background(Color.white)
        .onAppear {
            update_enrollment_list()
        }
    }
}

struct EnrollmentRow: View {
    let enrollment: UiEnrollmentChangeView.Enrollment
    let isSelected: Bool
    let onSelect: () -> Void
    
    var body: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text(enrollment.name)
                    .font(.system(size: 14, weight: .medium))
                    .foregroundColor(isSelected ? .white : .black)
                
                Text(enrollment.band_uuid)
                    .font(.system(size: 12))
                    .foregroundColor(isSelected ? .white.opacity(0.8) : .gray)
                    .lineLimit(1)
            }            
        }
        .padding(.vertical, 4)
        .contentShape(Rectangle())
        .background(isSelected ? Color.blue.opacity(0.1) : Color.clear)
        .cornerRadius(4)
    }
}

#Preview {
    UiEnrollmentChangeView()
}
