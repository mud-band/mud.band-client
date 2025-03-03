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

struct UiDashboardStatusListView: View {
    @EnvironmentObject private var mAppModel: AppModel
    let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()
    @State private var isLoading = false
    @State private var showTokenDialog = false
    @State private var enrollmentToken: String = ""
    @State private var isTokenLoading = false
    @State private var tokenError: String? = nil
    
    private var isConnected: Bool {
        return mAppModel.mVPNStatusString == "Connected"
    }
    
    private var statusColor: Color {
        switch mAppModel.mVPNStatusString {
        case "Connected":
            return .green
        case "Connecting", "Reasserting", "Not_ready":
            return .orange
        case "Disconnected", "Invalid":
            return .red
        default:
            return .gray
        }
    }
    
    private var displayStatusString: String {
        switch mAppModel.mVPNStatusString {
        case "Not_ready":
            return "Ready to Connect"
        default:
            return mAppModel.mVPNStatusString
        }
    }

    @ViewBuilder
    var body: some View {
        ZStack(alignment: .bottom) {
            VStack(spacing: 0) {
                ScrollView {
                    VStack(spacing: 12) {
                        statusCard
                        infoCard
                        if mAppModel.mBandAdminJsonString != nil &&
                            !mAppModel.mBandAdminJsonString!.isEmpty {
                            bandAdminCard
                        }
                        Spacer().frame(height: 80)
                    }
                    .padding(.horizontal)
                    .padding(.top, 12)
                }
            }
            connectionButton.padding()
        }
        .onReceive(timer) { _ in
            mAppModel.update_vpn_status()
            
            if isLoading && (mAppModel.mVPNStatusString == "Connected" || 
                            mAppModel.mVPNStatusString == "Disconnected" ||
                            mAppModel.mVPNStatusString == "Not_ready") {
                isLoading = false
            }
        }
        .sheet(isPresented: $showTokenDialog) {
            enrollmentTokenDialog
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
        }
        .padding(.vertical, 14)
        .padding(.horizontal, 16)
        .background(
            RoundedRectangle(cornerRadius: 16)
                .fill(Color(UIColor.systemBackground))
                .shadow(color: Color.black.opacity(0.12), radius: 6, x: 0, y: 3)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 16)
                .stroke(Color.blue.opacity(0.1), lineWidth: 1)
        )
    }
    
    private var statusCard: some View {
        HStack(spacing: 16) {
            Spacer()
            
            ZStack {
                Circle()
                    .fill(statusColor.opacity(0.2))
                    .frame(width: 70, height: 70)
                
                Circle()
                    .strokeBorder(statusColor, lineWidth: 2.5)
                    .frame(width: 70, height: 70)
                
                Image(systemName: isConnected ? "lock.shield.fill" : "lock.shield")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(width: 30, height: 30)
                    .foregroundColor(statusColor)
            }
            
            VStack(alignment: .leading) {
                Text("Status")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
                
                Text(displayStatusString)
                    .font(.title2)
                    .fontWeight(.medium)
                    .foregroundColor(statusColor)
            }
            
            Spacer()
        }
        .padding(.vertical, 8)
        .padding(.horizontal, 12)
    }
    
    private var infoCard: some View {
        VStack(spacing: 6){
            infoRow(icon: "antenna.radiowaves.left.and.right", title: "Band name", value: mAppModel.mBandName)
            
            if !mAppModel.mDeviceName.isEmpty {
                Divider().padding(.horizontal, 4)
                infoRow(icon: "desktopcomputer", title: "Device name", value: mAppModel.mDeviceName)
            }
            
            if !mAppModel.mPrivateIP.isEmpty {
                Divider().padding(.horizontal, 4)
                infoRow(icon: "network", title: "Private IP", value: mAppModel.mPrivateIP)
            }
        }
        .padding(.vertical, 10)
        .padding(.horizontal, 12)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(Color(UIColor.systemBackground))
                .shadow(color: Color.black.opacity(0.08), radius: 4, x: 0, y: 2)
        )
    }
    
    private func infoRow(icon: String, title: String, value: String) -> some View {
        HStack(alignment: .center, spacing: 12) {
            Image(systemName: icon)
                .frame(width: 20)
                .foregroundColor(.blue)
            
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.caption)
                    .foregroundColor(.secondary)
                
                Text(value)
                    .font(.subheadline)
                    .fontWeight(.medium)
            }
            
            Spacer()
        }
        .padding(.vertical, 4)
    }
    
    private var connectionButton: some View {
        Button(action: {
            isLoading = true
            if isConnected {
                mAppModel.mVpnManager.disconnectVPN()
            } else if mAppModel.mVPNStatusString == "Disconnected" || 
                     mAppModel.mVPNStatusString == "Not_ready" {
                mAppModel.mVpnManager.connectVPN()
            }
        }) {
            HStack {
                if isLoading {
                    ProgressView()
                        .progressViewStyle(CircularProgressViewStyle(tint: .white))
                        .frame(width: 20, height: 20)
                } else {
                    Image(systemName: isConnected ? "power" : "power.circle")
                }
                Text(isConnected ? "Disconnect" : "Connect")
            }
            .frame(minWidth: 0, maxWidth: .infinity)
            .padding(.vertical, 12)
            .background(isConnected ? Color.red : Color.blue)
            .foregroundColor(.white)
            .cornerRadius(12)
            .shadow(radius: 2)
        }
        .disabled(isLoading || (mAppModel.mVPNStatusString != "Connected" && 
                  mAppModel.mVPNStatusString != "Disconnected" && 
                  mAppModel.mVPNStatusString != "Not_ready"))
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
            
            // Bottom action buttons
            VStack(spacing: 8) {
                if !enrollmentToken.isEmpty {
                    copyButton
                }
                
                closeButton
            }
            .padding(.horizontal)
            .padding(.vertical, 12)
            .background(
                Rectangle()
                    .fill(Color(UIColor.systemBackground))
                    .shadow(color: Color.black.opacity(0.05), radius: 3, y: -2)
            )
        }
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
        }
    }
    
    private var successStateView: some View {
        VStack(spacing: 14) {
            // Visual indicator of success
            ZStack {
                Circle()
                    .fill(Color.green.opacity(0.1))
                    .frame(width: 56, height: 56)
                
                Image(systemName: "checkmark.circle.fill")
                    .font(.system(size: 28))
                    .foregroundColor(.green)
            }
            
            // Instructions
            Text("Use the following token to invite users to your Mud.band network")
                .font(.footnote)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal)
            
            // Token display
            VStack(spacing: 6) {
                Text("Invitation Token")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .frame(maxWidth: .infinity, alignment: .leading)
                
                HStack {
                    Text(enrollmentToken)
                        .font(.system(.callout, design: .monospaced))
                        .fontWeight(.medium)
                        .foregroundColor(.primary)
                    
                    Spacer()
                    
                    Button(action: {
                        UIPasteboard.general.string = enrollmentToken
                        
                        // Optional: Show a copy feedback
                        let generator = UINotificationFeedbackGenerator()
                        generator.notificationOccurred(.success)
                    }) {
                        Image(systemName: "doc.on.doc")
                            .font(.system(size: 16))
                            .foregroundColor(.blue)
                    }
                }
                .padding(10)
                .background(
                    RoundedRectangle(cornerRadius: 10)
                        .fill(Color(UIColor.secondarySystemBackground))
                )
            }
            
            // Additional guidance
            Text("This token is valid for 24 hours and can be used to register 100 users only.")
                .font(.caption2)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal)
        }
    }
    
    @ViewBuilder
    private var copyButton: some View {
        Button(action: {
            UIPasteboard.general.string = enrollmentToken
            
            // Add haptic feedback
            let generator = UINotificationFeedbackGenerator()
            generator.notificationOccurred(.success)
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
    }
    
    private var closeButton: some View {
        Button(action: {
            showTokenDialog = false
        }) {
            Text(enrollmentToken.isEmpty ? "Close" : "Done")
                .frame(minWidth: 0, maxWidth: .infinity)
                .padding(.vertical, 12)
                .background(Color(UIColor.secondarySystemBackground))
                .foregroundColor(.primary)
                .cornerRadius(10)
                .font(.headline)
        }
    }
}
