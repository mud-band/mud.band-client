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
import os

extension FileManager {
    private static var sharedFolderURL: URL? {
        guard let sharedFolderURL = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.band.mud") else {
            print("Cannot obtain shared folder URL")
            return nil
        }
        return sharedFolderURL
    }
    
    private static func getTodayString() -> String {
        let date = Date()
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "yyyyMMdd"
        return dateFormatter.string(from: date)
    }
    
    private static func createDir(_ path:String) {
        do {
            try FileManager.default.createDirectory(atPath: path, withIntermediateDirectories: true, attributes: nil)
        } catch let error as NSError {
            print("Unable to create directory \(error.debugDescription)")
        }
    }
    
    static func initMudbandAppGroupDirs() {
        if let url = sharedFolderURL {
            createDir(url.appendingPathComponent("admin").path)
            createDir(url.appendingPathComponent("enroll").path)
            createDir(url.appendingPathComponent("logs").path)
        }
    }
    
    static var TopDirURL: URL? {
        if let url = sharedFolderURL {
            return url
        }
        return nil
    }

    static var AdminDirURL: URL? {
        if let url = sharedFolderURL {
            return url.appendingPathComponent("admin")
        }
        return nil
    }

    static var EnrollDirURL: URL? {
        if let url = sharedFolderURL {
            return url.appendingPathComponent("enroll")
        }
        return nil
    }

    static var UiLogFileURL: URL? {
        if let url = sharedFolderURL {
            let dateString = getTodayString()
            return url.appendingPathComponent("logs/mudband-ui-\(dateString).txt")
        }
        return nil
    }
    
    static var TunnelLogFileURL: URL? {
        if let url = sharedFolderURL {
            let dateString = getTodayString()
            return url.appendingPathComponent("logs/mudband-tunnel-\(dateString).txt")
        }
        return nil
    }
    
    static func deleteFile(at url: URL) -> Bool {
        do {
            try FileManager.default.removeItem(at: url)
        } catch {
            return false
        }
        return true
    }
    
    static func deleteOldLogFiles() {
        for k in 3...60 {
            let currentDate = Date()
            let threeDaysAgo = Calendar.current.date(byAdding: .day, value: -k, to: currentDate)!
            let dateFormatter = DateFormatter()
            dateFormatter.dateFormat = "yyyyMMdd"
            let dateString = dateFormatter.string(from: threeDaysAgo)
            
            if let url = sharedFolderURL?.appendingPathComponent("mudband-ui-\(dateString).txt") {
                if !FileManager.deleteFile(at: url) {
                } else {
                }
            }
            if let url = sharedFolderURL?.appendingPathComponent("mudband-tunnel-\(dateString).txt") {
                if !FileManager.deleteFile(at: url) {
                } else {
                }
            }
        }
    }
    
    static func deleteAllLogFiles() {
        for k in 0...60 {
            let currentDate = Date()
            let threeDaysAgo = Calendar.current.date(byAdding: .day, value: -k, to: currentDate)!
            let dateFormatter = DateFormatter()
            dateFormatter.dateFormat = "yyyyMMdd"
            let dateString = dateFormatter.string(from: threeDaysAgo)
    
            if let url = sharedFolderURL?.appendingPathComponent("mudband-ui-\(dateString).txt") {
                if !FileManager.deleteFile(at: url) {
                } else {
                }
            }
            if let url = sharedFolderURL?.appendingPathComponent("mudband-tunnel-\(dateString).txt") {
                if !FileManager.deleteFile(at: url) {
                } else {
                }
            }
        }
    }
}
