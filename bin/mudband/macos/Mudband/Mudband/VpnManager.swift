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
import NetworkExtension
import SwiftyJSON
import SystemExtensions

class VpnManager: NSObject {
    private let LOG_LEVEL_ERROR: Int32 = 0
    private let LOG_LEVEL_WARNING: Int32 = 1
    private let LOG_LEVEL_INFO: Int32 = 2
    private let LOG_LEVEL_DEBUG: Int32 = 3
    
    private var mManager: NETunnelProviderManager?
    private var mSendPingTask: DispatchWorkItem?
    
    private var mPongMfaRequired = false
    private var mPongMfaURL = ""
    
    private func log(level: Int32, msg: String) {
        mudband_ui_log(level, msg)
    }
    
    public func connectVPN() {
        self.log(level: LOG_LEVEL_INFO, msg: "Try to connect.")
        self.createVPNTunnelProviderManager(completionHandler: { (error) -> Void in
            if let error = error {
                self.log(level: self.LOG_LEVEL_ERROR, msg: "BANDEC_00380: \(error)")
                return
            }
            self.controlVPN(cmd: "Connect")
        })
    }

    public func disconnectVPN() {
        self.log(level: LOG_LEVEL_INFO, msg: "Try to disconnect.")
        self.controlVPN(cmd: "Disconnect")
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
            if let task = self?.mSendPingTask {
                task.cancel()
            }
        }
    }
    
    private func controlVPN(cmd: String) {
        guard let manager = self.mManager else {
            self.log(level: self.LOG_LEVEL_ERROR, msg: "BANDEC_00381: No VPN manager found.")
            return
        }
        manager.loadFromPreferences { (error:Error?) in
            if let error = error {
                self.log(level: self.LOG_LEVEL_ERROR, msg: "BANDEC_00382: " + error.localizedDescription)
                return
            }
            switch cmd {
            case "Connect":
                do {
                    try manager.connection.startVPNTunnel()
                } catch {
                    self.log(level: self.LOG_LEVEL_ERROR, msg: "BANDEC_00383: " + error.localizedDescription)
                    return
                }
                self.log(level: self.LOG_LEVEL_INFO, msg: "Started the VPN tunnel.")
                break
            case "Disconnect":
                manager.connection.stopVPNTunnel()
                self.log(level: self.LOG_LEVEL_INFO, msg: "Stopped the VPN tunnel.")
                break
            default:
                self.log(level: self.LOG_LEVEL_ERROR, msg: "BANDEC_00384: Unexpected command \(cmd)")
                break
            }
        }
    }
    
    private func sendPing() {
        guard let manager = self.mManager else {
            self.log(level: self.LOG_LEVEL_ERROR, msg: "BANDEC_00385: No VPN manager found.")
            return
        }
        if let session = manager.connection as? NETunnelProviderSession,
           let message = "ping".data(using: String.Encoding.utf8),
           manager.connection.status != .invalid {
            do {
                try session.sendProviderMessage(message) { response in
                    guard let responseData = response else {
                        self.log(level: self.LOG_LEVEL_WARNING,
                                 msg: "BANDEC_00386: Got a nil response from the provider for ping cmd.")
                        return
                    }
                    if let obj = try? JSON(data: responseData) {
                        switch obj["status"].intValue {
                        case 200:
                            break
                        case 301:
                            self.mPongMfaRequired = true
                            self.mPongMfaURL = obj["sso_url"].stringValue
                        default:
                            let msg = obj["msg"].stringValue
                            self.log(level: self.LOG_LEVEL_WARNING, msg: "BANDEC_00505: \(msg)")
                            return
                        }
                    }
                }
            } catch {
                self.log(level: self.LOG_LEVEL_ERROR,
                         msg: "BANDEC_00387: Failed to send a message to the provider")
            }
        }
        
        self.mSendPingTask = DispatchWorkItem { [weak self] in
            self?.sendPing()
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 3.0, execute: self.mSendPingTask!)
    }
    
    func getVPNStatusString() -> String {
        guard let manager = self.mManager else {
            return "Not_ready"
        }
        switch manager.connection.status {
        case .connecting:
            return "Connecting"
        case .connected:
            return "Connected"
        case .disconnecting:
            return "Disconnecting"
        case .disconnected:
            return "Disconnected"
        case .invalid:
            return "Invalid"
        case .reasserting:
            return "Reasserting"
        default:
            break
        }
        return "Unknown"
    }
    
    @objc func VPNStatusDidChange(_ notification: Notification?) {
        guard let manager = self.mManager else {
            return
        }
        var msg = "VPN Status changed: "
        let status = manager.connection.status
        switch status {
        case .connecting:
            msg += "Connecting..."
            break
        case .connected:
            msg += "Connected..."
            break
        case .disconnecting:
            msg += "Disconnecting..."
            break
        case .disconnected:
            msg += "Disconnected..."
            break
        case .invalid:
            msg += "Invalid..."
            break
        case .reasserting:
            msg += "Reasserting..."
            break
        default:
            msg += "unexpected"
            break
        }
        self.log(level: LOG_LEVEL_INFO, msg: msg)
    }
    
    public func initVPNTunnelProviderManager() {
        self.log(level: self.LOG_LEVEL_INFO, msg: "Initialize the notification center.")
        NotificationCenter.default.addObserver(self,
                                               selector: #selector(VpnManager.VPNStatusDidChange(_:)),
                                               name: NSNotification.Name.NEVPNStatusDidChange, object: nil)
        NETunnelProviderManager.loadAllFromPreferences { (savedManagers: [NETunnelProviderManager]?, error: Error?) in
            if let error = error {
                self.log(level: self.LOG_LEVEL_ERROR,
                         msg: "BANDEC_00388: Failed to load the all VPN profiles: \(error.localizedDescription)")
                return
            }
            if let savedManagers = savedManagers {
                if savedManagers.count > 0 {
                    self.mManager = savedManagers[0]
                    self.log(level: self.LOG_LEVEL_INFO, msg: "Okay.  Found a VPN manager for this app.")
                }
            }
        }
    }
        
    func createVPNTunnelProviderManager(completionHandler: @escaping (Error?) -> Void) {
        if let _ = self.mManager {
            completionHandler(nil)
            return
        } else {
            self.mManager = NETunnelProviderManager()
            if let manager = self.mManager {
                manager.loadFromPreferences(completionHandler: { (error:Error?) in
                    if let error = error {
                        self.log(level: self.LOG_LEVEL_ERROR,
                                 msg: "BANDEC_00389: Failed to load the mud.band VPN profile: \(error.localizedDescription)")
                        completionHandler(error)
                        return
                    }
                    let providerProtocol = NETunnelProviderProtocol()
                    providerProtocol.providerBundleIdentifier = "net.mudfish.mudband.Mudband.Mudband-Tunnel"
                    providerProtocol.providerConfiguration = [:]
                    providerProtocol.serverAddress = "127.0.0.1"
                    manager.protocolConfiguration = providerProtocol
                    manager.localizedDescription = "Mud.band Tunnel"
                    manager.isEnabled = true
                    manager.saveToPreferences(completionHandler: { (error:Error?) in
                        if let error = error {
                            self.log(level: self.LOG_LEVEL_ERROR,
                                     msg: "BANDEC_00390: Failed to update the Mudband profile: \(error.localizedDescription)")
                            completionHandler(error)
                            return
                        }
                        self.log(level: self.LOG_LEVEL_INFO, msg: "Synced")
                        completionHandler(nil)
                    })
                })
            } else {
                self.log(level: self.LOG_LEVEL_ERROR,
                         msg: "BANDEC_00391: NETunnelProviderManager() failed")
            }
        }
    }
}
