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

struct UiEnrollmentIntroView: View {
    var body: some View {
        NavigationStack {
            ScrollView {
                Spacer()
                VStack(spacing: 16) {
                    VStack(spacing: 8) {
                        Image(systemName: "globe")
                            .resizable()
                            .aspectRatio(contentMode: .fit)
                            .frame(width: 50, height: 50)
                            .foregroundStyle(.tint)
                            .padding(.vertical, 8)
                            .background(
                                Circle()
                                    .fill(Color.blue.opacity(0.1))
                            )
                        
                        Text("Welcome to Mud.band")
                            .font(.title)
                            .fontWeight(.bold)
                        
                        Text("Choose an option to get started")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                            .multilineTextAlignment(.center)
                            .padding(.bottom, 4)
                    }
                    .padding(.top, 12)
                    Spacer()
                    
                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            Image(systemName: "person.badge.plus")
                                .font(.title3)
                                .foregroundColor(.blue)
                            Text("Join Existing Band")
                                .font(.headline)
                        }
                        
                        Text("Connect to a band that has already been created")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                        
                        NavigationLink(destination: UiEnrollmentNewView()) {
                            HStack {
                                Text("Enroll")
                                Spacer()
                                Image(systemName: "arrow.right")
                            }
                            .padding(.vertical, 8)
                            .padding(.horizontal, 12)
                            .cornerRadius(8)
                        }
                    }
                    .padding(12)
                    .background(
                        RoundedRectangle(cornerRadius: 12)
                            .fill(backgroundColor)
                            .shadow(color: Color.black.opacity(0.1), radius: 3, x: 0, y: 1)
                    )
                    .padding(.horizontal)
                    
                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            Image(systemName: "plus.circle")
                                .font(.title3)
                                .foregroundColor(.green)
                            Text("Create New Band")
                                .font(.headline)
                        }
                        
                        Text("Start your own band and invite others to join")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                        
                        NavigationLink(destination: UiBandCreateAsGuestView()) {
                            HStack {
                                Text("Create Band as Guest")
                                Spacer()
                                Image(systemName: "arrow.right")
                            }
                            .padding(.vertical, 8)
                            .padding(.horizontal, 12)
                            .cornerRadius(8)
                        }
                        
                        Link(destination: URL(string: "https://mud.band")!) {
                            HStack {
                                Text("Create Band")
                                Spacer()
                                Image(systemName: "arrow.up.right")
                            }
                            .foregroundColor(.white)
                            .padding(.vertical, 8)
                            .padding(.horizontal, 12)
                            .background(Color.green)
                            .cornerRadius(8)
                        }
                    }
                    .padding(12)
                    .background(
                        RoundedRectangle(cornerRadius: 12)
                            .fill(backgroundColor)
                            .shadow(color: Color.black.opacity(0.1), radius: 3, x: 0, y: 1)
                    )
                    .padding(.horizontal)
                }
            }
            .navigationTitle("Mudband")
        }
    }
    
    private var backgroundColor: Color {
        return Color.gray.opacity(0.05)
    }
}
