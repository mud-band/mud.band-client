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
    @Environment(\.colorScheme) var colorScheme
    
    struct Enrollment: Identifiable {
        var id = UUID()
        var name: String
        var band_uuid: String
        var isActive: Bool = false
    }
    @State var enrollments: [Enrollment] = []
    @State private var isLoading: Bool = true
    @State private var showingConfirmation: Bool = false
    @State private var selectedEnrollment: Enrollment?

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
        isLoading = true
        
        guard let band_uuids = mudband_ui_enroll_get_band_uuids() as? [String] else {
            mudband_ui_log(0, "BANDEC_00275: mudband_ui_enroll_get_band_uuids() failed.")
            isLoading = false
            return
        }
        
        let currentDefault = mudband_ui_progconf_get_default_band_uuid_objc() ?? ""
        
        for band_uuid in band_uuids {
            guard let obj = read_band_json(band_uuid: band_uuid) else {
                continue
            }
            enrollments.append(Enrollment(
                name: obj["name"].stringValue,
                band_uuid: band_uuid,
                isActive: band_uuid == currentDefault
            ))
        }
        
        isLoading = false
    }
    
    private func change_enrollment_default_band_uuid(band_uuid: String) {
        mudband_ui_progconf_set_default_band_uuid_objc(band_uuid)
        mAppModel.update_enrollments()
        self.dismiss()
    }
    
    @ViewBuilder
    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                VStack(spacing: 8) {
                    Image(systemName: "network")
                        .font(.system(size: 45))
                        .foregroundStyle(.blue)
                        .padding(.top, 10)
                    
                    Text("Band Selection")
                        .font(.headline)
                        .fontWeight(.bold)
                    
                    Text("Choose which band you want to change to")
                        .font(.caption)
                        .multilineTextAlignment(.center)
                        .foregroundStyle(.secondary)
                        .padding(.horizontal, 16)
                        .padding(.bottom, 5)
                }
                .padding(.vertical, 8)
                
                if isLoading {
                    Spacer()
                    ProgressView("Loading enrollments...")
                    Spacer()
                } else if enrollments.isEmpty {
                    Spacer()
                    VStack(spacing: 20) {
                        Image(systemName: "network.slash")
                            .font(.system(size: 56))
                            .foregroundColor(.secondary)
                        Text("No enrollments found")
                            .font(.headline)
                        Text("You need to enroll in at least one band to get started.")
                            .multilineTextAlignment(.center)
                            .foregroundColor(.secondary)
                            .padding(.horizontal)
                    }
                    .padding()
                    Spacer()
                } else {
                    Form {
                        Section(header: Text("AVAILABLE BANDS").font(.caption)) {
                            ForEach(enrollments) { enrollment in
                                Button(action: {
                                    selectedEnrollment = enrollment
                                    showingConfirmation = true
                                }) {
                                    HStack(spacing: 16) {
                                        Image(systemName: "network")
                                            .font(.system(size: 20))
                                            .foregroundStyle(.blue)
                                            .frame(width: 20)
                                        
                                        VStack(alignment: .leading, spacing: 4) {
                                            Text(enrollment.name)
                                                .font(.headline)
                                            
                                            Text(enrollment.band_uuid)
                                                .font(.caption)
                                                .foregroundColor(.secondary)
                                                .lineLimit(1)
                                        }
                                        
                                        Spacer()
                                        
                                        if enrollment.isActive {
                                            Text("Active")
                                                .font(.caption)
                                                .padding(.horizontal, 8)
                                                .padding(.vertical, 4)
                                                .background(Color.blue)
                                                .foregroundColor(.white)
                                                .clipShape(Capsule())
                                        } else {
                                            Image(systemName: "checkmark.circle")
                                                .font(.system(size: 20))
                                                .foregroundColor(.blue)
                                                .opacity(0)
                                        }
                                    }
                                    .padding(.vertical, 2)
                                }
                                .buttonStyle(PlainButtonStyle())
                            }
                        }
                        .listRowBackground(colorScheme == .dark ? Color(UIColor.secondarySystemBackground) : Color(UIColor.systemBackground))
                    }
                    .scrollContentBackground(.hidden)                    
                }
            }
            .navigationTitle("Band Selection")
            .navigationBarTitleDisplayMode(.inline)
            .onAppear {
                update_enrollment_list()
            }
            .alert(isPresented: $showingConfirmation) {
                Alert(
                    title: Text("Change Band"),
                    message: Text("Do you want to switch to '\(selectedEnrollment?.name ?? "")'?"),
                    primaryButton: .default(Text("Switch")) {
                        if let enrollment = selectedEnrollment {
                            change_enrollment_default_band_uuid(band_uuid: enrollment.band_uuid)
                        }
                    },
                    secondaryButton: .cancel()
                )
            }
        }
    }
}

#Preview {
    UiEnrollmentChangeView()
}
