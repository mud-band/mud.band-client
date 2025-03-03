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

struct UiEnrollmentIntro: View {
    @EnvironmentObject private var mAppModel: AppModel
    @Environment(\.dismiss) var dismiss
    @Environment(\.colorScheme) var colorScheme
    
    // 역할별 색상 정의
    private let enrollColor = Color.blue
    private let createGuestColor = Color.green
    private let createAccountColor = Color.purple
    
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 16) {
                    VStack(spacing: 5) {
                        Image(systemName: "globe")
                            .font(.system(size: 45))
                            .foregroundStyle(.tint)
                        
                        Text("Welcome to Mud.band")
                            .font(.title3)
                            .bold()
                    }
                    .padding(.vertical, 12)
                    
                    SectionCard(
                        title: "Join an Existing Band",
                        icon: "person.2.fill",
                        description: "Connect to the mesh network by enrolling in an existing band."
                    ) {
                        NavigationLink(destination: UiEnrollmentNewView()) {
                            HStack {
                                Text("Enroll")
                                Image(systemName: "arrow.right")
                            }
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 8)
                            .background(enrollColor)
                            .foregroundColor(.white)
                            .cornerRadius(8)
                        }
                    }
                    
                    SectionCard(
                        title: "Create a New Band",
                        icon: "plus.circle.fill",
                        description: "Start your own network by creating a new band."
                    ) {
                        VStack(spacing: 10) {
                            NavigationLink(destination: UiBandCreateAsGuestView()) {
                                HStack {
                                    Text("Create Band as Guest")
                                    Image(systemName: "arrow.right")
                                }
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 8)
                                .background(createGuestColor)
                                .foregroundColor(.white)
                                .cornerRadius(8)
                            }
                            
                            Link(destination: URL(string: "https://mud.band")!) {
                                HStack {
                                    Text("Create with Account")
                                    Image(systemName: "arrow.up.forward.square")
                                }
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 8)
                                .background(createAccountColor)
                                .foregroundColor(.white)
                                .cornerRadius(8)
                            }
                        }
                    }
                }
                .padding(12)
            }
            .navigationTitle("Mud.band")
            .navigationBarTitleDisplayMode(.inline)
        }
    }
}

// Helper view for consistent section styling
struct SectionCard<Content: View>: View {
    let title: String
    let icon: String
    let description: String
    let content: Content
    
    init(title: String, icon: String, description: String, @ViewBuilder content: () -> Content) {
        self.title = title
        self.icon = icon
        self.description = description
        self.content = content()
    }
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Image(systemName: icon)
                    .font(.title3)
                    .foregroundColor(.accentColor)
                
                Text(title)
                    .font(.headline)
                    .bold()
            }
            
            Text(description)
                .font(.subheadline)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            
            content
        }
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(Color(UIColor.systemBackground))
                .shadow(color: Color.black.opacity(0.1), radius: 5, x: 0, y: 3)
        )
    }
}
