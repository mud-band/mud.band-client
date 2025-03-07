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
import Foundation
import SwiftUI
import SwiftyJSON

struct UiDashboardStatusListView: View {
    @EnvironmentObject private var mAppModel: AppModel
    @State private var showTokenDialog = false
    @State private var enrollmentToken: String = ""
    @State private var isTokenLoading = false
    @State private var tokenError: String? = nil
        
    let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    @ViewBuilder
    var body: some View {
        ZStack {
            VStack(spacing: 12) {
                if mAppModel.mBandAdminJsonString != nil &&
                    !mAppModel.mBandAdminJsonString!.isEmpty {
                    bandAdminCard
                }

                // Header
                VStack(spacing: 8) {
                    Image(systemName: "network")
                        .font(.system(size: 32))
                        .foregroundColor(.accentColor)
                        .frame(width: 36, height: 36)
                }
                .padding(.top, 12)
                
                // Connection Details (now using the separated view)
                ConnectionDetailsView()
                
                Spacer()
            }
            
            VStack {
                Spacer()
                connectionButton
                    .padding(.horizontal)
                    .padding(.bottom, 20)
            }
        }
        .padding(12)
        .onReceive(timer) { _ in
            mAppModel.update_vpn_status()
        }
        .sheet(isPresented: $showTokenDialog) {
            enrollmentTokenDialog
        }
    }
    
    // Helper Views
    private var connectionStatusIcon: String {
        if mAppModel.mVPNStatusString == "Connected" {
            return "checkmark.circle.fill"
        } else if mAppModel.mVPNStatusString == "Disconnected" {
            return "x.circle.fill"
        } else {
            return "ellipsis.circle.fill"
        }
    }
    
    private var connectionStatusColor: Color {
        if mAppModel.mVPNStatusString == "Connected" {
            return .green
        } else if mAppModel.mVPNStatusString == "Disconnected" {
            return .red
        } else {
            return .orange
        }
    }
    
    private var bandAdminCard: some View {
        VStack(spacing: 12) {
            HStack {
                Image(systemName: "person.2.badge.gearshape.fill")
                    .font(.title2)
                    .foregroundColor(.blue)
                    .padding(8)
                    .background(Color.blue.opacity(0.1))
                    .clipShape(Circle())
                
                Text("Band Admin")
                    .font(.headline)
                    .fontWeight(.bold)
            }
            .padding(.horizontal, 4)
            
            Divider()
                .padding(.horizontal, 4)
                .padding(.vertical, 4)
            
            Text("Manage your band settings and invite new members to join your network.")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 4)
                .padding(.bottom, 4)

            Button(action: {
                fetchEnrollmentToken()
            }) {
                HStack {
                    Image(systemName: "key.fill")
                        .font(.subheadline)
                    Text("Get enrollment token")
                }
                .frame(minWidth: 0, maxWidth: .infinity)
                .padding(.vertical, 12)
                .background(
                    RoundedRectangle(cornerRadius: 10)
                        .fill(Color.blue.opacity(0.1))
                )
                .foregroundColor(.blue)
                .cornerRadius(10)
                .overlay(
                    RoundedRectangle(cornerRadius: 10)
                        .stroke(Color.blue.opacity(0.3), lineWidth: 1)
                )
            }
            .buttonStyle(PlainButtonStyle())
        }
        .padding(.vertical, 14)
        .padding(.horizontal, 16)
        .background(
            RoundedRectangle(cornerRadius: 16)
                .shadow(color: Color.black.opacity(0.12), radius: 6, x: 0, y: 3)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 16)
                .stroke(Color.blue.opacity(0.1), lineWidth: 1)
        )
    }

    private var loadingStateView: some View {
        VStack(spacing: 12) {
            ProgressView()
                .scaleEffect(1.1)
            
            Text("Fetching token...")
                .font(.subheadline)
                .foregroundColor(.secondary)
        }
    }
    
    private func errorStateView(_ error: String) -> some View {
        VStack(spacing: 12) {
            Image(systemName: "exclamationmark.triangle.fill")
                .font(.system(size: 32))
                .foregroundColor(.orange)
            
            Text("An error occurred")
                .font(.headline)
                .foregroundColor(.primary)
            
            Text(error)
                .font(.subheadline)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
            
            Button(action: {
                fetchEnrollmentToken()
            }) {
                HStack {
                    Image(systemName: "arrow.counterclockwise")
                    Text("Try Again")
                }
                .font(.subheadline)
                .foregroundColor(.blue)
                .padding(.horizontal, 14)
                .padding(.vertical, 6)
                .background(
                    Capsule()
                        .stroke(Color.blue, lineWidth: 1)
                )
            }
            .buttonStyle(PlainButtonStyle())
        }
    }
        
    private var successStateView: some View {
        VStack(spacing: 20) {
            // Success indicator
            Image(systemName: "checkmark.circle.fill")
                .font(.system(size: 40))
                .foregroundColor(.green)
                .padding(.top, 8)
            
            // Primary instruction
            Text("Enrollment Token Generated")
                .font(.headline)
                .fontWeight(.bold)
            
            // Token display with prominent styling
            VStack(spacing: 8) {
                // Token container
                VStack(spacing: 12) {
                    Text(enrollmentToken)
                        .font(.system(.body, design: .monospaced))
                        .fontWeight(.medium)
                        .padding(.horizontal, 16)
                        .padding(.vertical, 12)
                        .background(
                            RoundedRectangle(cornerRadius: 8)
                                .fill(Color.blue.opacity(0.1))
                        )
                        .overlay(
                            RoundedRectangle(cornerRadius: 8)
                                .stroke(Color.blue.opacity(0.3), lineWidth: 1)
                        )
                        .multilineTextAlignment(.center)
                        .textSelection(.enabled)
                    
                    // Copy button
                    Button(action: {
                        let pasteboard = NSPasteboard.general
                        pasteboard.clearContents()
                        pasteboard.setString(enrollmentToken, forType: .string)
                        
                        // Optional feedback
                        NSSound.beep()
                    }) {
                        HStack(spacing: 6) {
                            Image(systemName: "doc.on.clipboard")
                            Text("Copy to Clipboard")
                        }
                        .font(.subheadline)
                        .fontWeight(.medium)
                        .foregroundColor(.white)
                        .padding(.horizontal, 20)
                        .padding(.vertical, 8)
                        .background(Color.blue)
                        .cornerRadius(6)
                    }
                    .buttonStyle(PlainButtonStyle())
                }
                .padding(.horizontal, 12)
            }
            
            // Instructions
            VStack(spacing: 10) {
                Text("Share this token with users you want to invite to your Mud.band network")
                    .font(.subheadline)
                    .foregroundColor(.primary)
                    .multilineTextAlignment(.center)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal)
                
                // Token validity information
                HStack(spacing: 4) {
                    Image(systemName: "clock")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    Text("Valid for 24 hours, up to 100 users")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                .padding(.top, 4)
            }
            
            Spacer()
        }
        .padding(.vertical, 10)
    }
    
    @ViewBuilder
    private var copyButton: some View {
        Button(action: {
            let pasteboard = NSPasteboard.general
            pasteboard.clearContents()
            pasteboard.setString(enrollmentToken, forType: .string)
            
            // Add feedback for macOS
            NSSound.beep()
        }) {
            HStack {
                Image(systemName: "doc.on.doc.fill")
                Text("Copy Token")
            }
            .frame(minWidth: 0, maxWidth: .infinity)
            .padding(.vertical, 12)
            .background(Color.blue)
            .foregroundColor(.white)
            .cornerRadius(10)
            .font(.headline)
        }
        .buttonStyle(PlainButtonStyle())
    }
    
    private var closeButton: some View {
        Button(action: {
            showTokenDialog = false
        }) {
            Text(enrollmentToken.isEmpty ? "Close" : "Done")
                .frame(minWidth: 0, maxWidth: .infinity)
                .padding(.vertical, 12)
                .background(Color(NSColor.controlBackgroundColor))
                .foregroundColor(.primary)
                .cornerRadius(10)
                .font(.headline)
        }
        .buttonStyle(PlainButtonStyle())
    }
    
    private var enrollmentTokenDialog: some View {
        VStack(spacing: 0) {
            // Header
            HStack {
                Text("Enrollment Token")
                    .font(.headline)
                    .fontWeight(.bold)
                Spacer()
                Button(action: {
                    showTokenDialog = false
                }) {
                    Image(systemName: "xmark.circle.fill")
                        .font(.title3)
                        .foregroundColor(.gray)
                }
                .buttonStyle(BorderlessButtonStyle())
            }
            .padding(.horizontal)
            .padding(.top, 16)
            .padding(.bottom, 8)
            
            Divider()
            
            ScrollView {
                VStack(spacing: 12) {
                    // Content based on state
                    if isTokenLoading {
                        loadingStateView
                            .padding(.vertical, 20)
                    } else if let error = tokenError {
                        errorStateView(error)
                            .padding(.vertical, 16)
                    } else if !enrollmentToken.isEmpty {
                        successStateView
                            .padding(.vertical, 12)
                    }
                }
                .padding(.horizontal)
                .padding(.vertical, 8)
            }
        }
        .frame(width: 400, height: 400)
    }

    private func fetchEnrollmentToken() {
        showTokenDialog = true
        isTokenLoading = true
        enrollmentToken = ""
        tokenError = nil
        
        Task {
            guard let bandAdminJsonString = mAppModel.mBandAdminJsonString, !bandAdminJsonString.isEmpty else {
                isTokenLoading = false
                tokenError = "Band admin data not available"
                return
            }
            
            do {
                // Parse JSON on background thread
                let jwtToken = try await Task.detached(priority: .userInitiated) {
                    guard let jsonData = bandAdminJsonString.data(using: .utf8) else {
                        throw NSError(domain: "EnrollmentError", code: 1, userInfo: [NSLocalizedDescriptionKey: "Invalid band admin data format"])
                    }
                    
                    let json = try JSON(data: jsonData)
                    guard let token = json["jwt"].string else {
                        throw NSError(domain: "EnrollmentError", code: 2, userInfo: [NSLocalizedDescriptionKey: "JWT token not found in band admin data"])
                    }
                    
                    return token
                }.value
                
                let headers: HTTPHeaders = [
                    "Authorization": jwtToken
                ]
                
                // Use Alamofire with async/await
                let response = await AF.request(
                    "https://www.mud.band/api/band/anonymous/enrollment/token",
                    method: .get,
                    headers: headers
                ).serializingData().response
                
                // Process response on main thread
                guard let responseData = response.data else {
                    throw NSError(domain: "EnrollmentError", code: 3, userInfo: [NSLocalizedDescriptionKey: "No data received from server"])
                }
                let json = try JSON(data: responseData)
                
                await MainActor.run {
                    isTokenLoading = false
                    
                    if json["status"].intValue == 200 {
                        enrollmentToken = json["token_uuid"].stringValue
                    } else {
                        tokenError = "Failed to get token: \(json["msg"].stringValue)"
                    }
                }
            } catch {
                await MainActor.run {
                    isTokenLoading = false
                    tokenError = "Error: \(error.localizedDescription)"
                }
            }
        }
    }
        
    private var connectionButton: some View {
        Group {
            if mAppModel.mVPNStatusString == "Connected" {
                Button(action: {
                    mAppModel.mVpnManager.disconnectVPN()
                }) {
                    HStack {
                        Image(systemName: "xmark.circle.fill")
                            .frame(width: 18, height: 18)
                        Text("Disconnect")
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .background(Color.red)
                    .foregroundColor(.white)
                    .cornerRadius(6)
                }
                .buttonStyle(PlainButtonStyle())
            } else if mAppModel.mVPNStatusString == "Disconnected" || mAppModel.mVPNStatusString == "Not_ready" {
                Button(action: {
                    mAppModel.mVpnManager.connectVPN()
                }) {
                    HStack {
                        Image(systemName: "lock.shield.fill")
                            .frame(width: 18, height: 18)
                        Text("Connect")
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(6)
                }
                .buttonStyle(PlainButtonStyle())
            } else {
                Button(action: {}) {
                    HStack {
                        Image(systemName: "ellipsis.circle.fill")
                            .frame(width: 18, height: 18)
                        Text(mAppModel.mVPNStatusString)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .background(Color.gray.opacity(0.5))
                    .foregroundColor(.white)
                    .cornerRadius(6)
                }
                .buttonStyle(PlainButtonStyle())
                .disabled(true)
            }
        }
    }
}

// New separated view for Connection Details
struct ConnectionDetailsView: View {
    @EnvironmentObject private var mAppModel: AppModel
    
    var body: some View {
        VStack(spacing: 12) {
            HStack {
                Label {
                    Text("Connection Details")
                        .font(.headline)
                        .foregroundColor(.primary)
                } icon: {
                    Image(systemName: "info.circle")
                        .foregroundColor(.accentColor)
                        .frame(width: 18, height: 18)
                }
                Spacer()
            }
            .padding(.bottom, 4)
            
            // Connection Details
            VStack(spacing: 8) {
                HStack {
                    Image(systemName: "antenna.radiowaves.left.and.right")
                        .foregroundColor(.accentColor)
                        .frame(width: 18, height: 18)
                    
                    Text("Band Name:")
                        .foregroundColor(.secondary)
                        .lineLimit(1)
                    
                    Spacer(minLength: 8)
                    
                    Text(mAppModel.mBandName)
                        .fontWeight(.medium)
                        .foregroundColor(.primary)
                        .textSelection(.enabled)
                        .lineLimit(1)
                }
                .padding(.vertical, 2)
                
                // Device Info
                if !mAppModel.mDeviceName.isEmpty {
                    HStack {
                        Image(systemName: "desktopcomputer")
                            .foregroundColor(.accentColor)
                            .frame(width: 18, height: 18)
                        
                        Text("Device Name:")
                            .foregroundColor(.secondary)
                            .lineLimit(1)
                        
                        Spacer(minLength: 8)
                        
                        Text(mAppModel.mDeviceName)
                            .fontWeight(.medium)
                            .foregroundColor(.primary)
                            .textSelection(.enabled)
                            .lineLimit(1)
                    }
                    .padding(.vertical, 2)
                }
                
                // IP Address
                if !mAppModel.mPrivateIP.isEmpty {
                    HStack {
                        Image(systemName: "network")
                            .foregroundColor(.accentColor)
                            .frame(width: 18, height: 18)
                        
                        Text("Private IP:")
                            .foregroundColor(.secondary)
                            .lineLimit(1)
                        
                        Spacer(minLength: 8)
                        
                        Text(mAppModel.mPrivateIP)
                            .fontWeight(.medium)
                            .foregroundColor(.primary)
                            .textSelection(.enabled)
                            .lineLimit(1)
                    }
                    .padding(.vertical, 2)
                }
                                    
                // Connection Status
                HStack {
                    Image(systemName: connectionStatusIcon)
                        .foregroundColor(connectionStatusColor)
                        .frame(width: 18, height: 18)
                    
                    Text("Status:")
                        .foregroundColor(.secondary)
                        .lineLimit(1)
                    
                    Spacer(minLength: 8)
                    
                    Text(mAppModel.mVPNStatusString)
                        .fontWeight(.medium)
                        .foregroundColor(connectionStatusColor)
                        .textSelection(.enabled)
                        .lineLimit(1)
                }
                .padding(.vertical, 2)
            }
        }
        .padding(.horizontal)
    }
    
    // Helper computed properties moved from main view
    private var connectionStatusIcon: String {
        if mAppModel.mVPNStatusString == "Connected" {
            return "checkmark.circle.fill"
        } else if mAppModel.mVPNStatusString == "Disconnected" {
            return "x.circle.fill"
        } else {
            return "ellipsis.circle.fill"
        }
    }
    
    private var connectionStatusColor: Color {
        if mAppModel.mVPNStatusString == "Connected" {
            return .green
        } else if mAppModel.mVPNStatusString == "Disconnected" {
            return .red
        } else {
            return .orange
        }
    }
}
