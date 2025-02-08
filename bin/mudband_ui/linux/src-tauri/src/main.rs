#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::sync::Mutex;
use std::fs;
use std::os::unix::net::UnixStream;
use std::io::{Write, Read};
use serde::{Deserialize, Serialize};
use tauri::Manager;
use dirs;

#[derive(Default, Serialize, Deserialize)]
struct AppState {
    #[serde(default = "default_user_tos_agreed")]
    user_tos_agreed: bool,
}

fn default_user_tos_agreed() -> bool {
    false
}

fn get_config_path() -> std::path::PathBuf {
    let mut config_dir = dirs::home_dir().unwrap_or_default();
    config_dir.push(".config");
    config_dir.push("mudband");
    config_dir.push("ui");
    fs::create_dir_all(&config_dir).unwrap_or_default();
    config_dir.push("mudband_state.json");
    config_dir
}

fn load_config() -> AppState {
    match fs::read_to_string(get_config_path()) {
        Ok(contents) => {
            serde_json::from_str(&contents).unwrap_or_default()
        }
        Err(_) => AppState::default()
    }
}

fn save_config(config: &AppState) -> std::io::Result<()> {
    let contents = serde_json::to_string_pretty(config)?;
    fs::write(get_config_path(), contents)
}

#[tauri::command]
fn mudband_ui_is_user_tos_agreed(state: tauri::State<'_, Mutex<AppState>>) -> bool {
    let state = state.lock().unwrap();
    return state.user_tos_agreed;
}

#[tauri::command]
fn mudband_ui_set_user_tos_agreed(state: tauri::State<'_, Mutex<AppState>>, agreed: bool) {
    let mut state = state.lock().unwrap();
    state.user_tos_agreed = agreed;
    let _ = save_config(&state);
}

fn mudband_ui_ipc_send(command: serde_json::Value) -> Result<serde_json::Value, String> {
    let socket_path = "/var/run/mudband_service.sock";
    
    let mut stream = UnixStream::connect(socket_path)
        .map_err(|e| format!("BANDEC_00587: Failed to connect to socket: {}", e))?;
    
    let cmd_str = serde_json::to_string(&command).unwrap();
    
    stream.write_all(cmd_str.as_bytes())
        .map_err(|e| format!("BANDEC_00588: Failed to write to socket: {}", e))?;
    
    let mut response = String::new();
    stream.read_to_string(&mut response)
        .map_err(|e| format!("BANDEC_00589: Failed to read from socket: {}", e))?;
    
    serde_json::from_str(&response)
        .map_err(|e| format!("BANDEC_00590: Failed to parse response as JSON: {}", e))
}

#[tauri::command]
fn mudband_ui_tunnel_connect(_state: tauri::State<'_, Mutex<AppState>>) -> bool {
    let command = serde_json::json!({
        "cmd": "tunnel_connect"
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            if json.get("status").and_then(|s| s.as_u64()) != Some(200) {
                println!("BANDEC_00591: Invalid status code");
                return false;
            }
            true
        }
        Err(e) => {
            println!("BANDEC_00592: {}", e);
            false
        }
    }
}

#[tauri::command]
fn mudband_ui_tunnel_disconnect(_state: tauri::State<'_, Mutex<AppState>>) -> bool {
    let command = serde_json::json!({
        "cmd": "tunnel_disconnect"
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            if json.get("status").and_then(|s| s.as_u64()) != Some(200) {
                println!("BANDEC_00593: Invalid status code");
                return false;
            }
            true
        }
        Err(e) => {
            println!("BANDEC_00594: {}", e);
            false
        }
    }
}

#[tauri::command]
fn mudband_ui_tunnel_is_running(_state: tauri::State<'_, Mutex<AppState>>) -> bool {
    let command = serde_json::json!({
        "cmd": "tunnel_get_status"
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            if json.get("status").and_then(|s| s.as_u64()) != Some(200) {
                println!("BANDEC_00595: Invalid status code");
                return false;
            }
            json.get("tunnel_is_running")
                .and_then(|status| status.as_bool())
                .unwrap_or(false)
        }
        Err(e) => {
            println!("BANDEC_00596: {}", e);
            false
        }
    }
}

#[tauri::command]
fn mudband_ui_get_enrollment_count(_state: tauri::State<'_, Mutex<AppState>>) -> i64 {
    let command = serde_json::json!({
        "cmd": "get_enrollment_count"
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            if json.get("status").and_then(|s| s.as_u64()) != Some(200) {
                println!("BANDEC_00597: Invalid status code");
                return 0;
            }
            json.get("enrollment_count")
                .and_then(|count| count.as_i64())
                .unwrap_or(0)
        }
        Err(e) => {
            println!("BANDEC_00598: {}", e);
            -1
        }
    }
}

#[tauri::command]
fn mudband_ui_get_band_name(_state: tauri::State<'_, Mutex<AppState>>) -> String {
    let command = serde_json::json!({
        "cmd": "get_band_name",
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            if json.get("status").and_then(|s| s.as_u64()) != Some(200) {
                return "BANDEC_00599: Invalid status code".to_string();
            }
            json.get("band_name")
                .and_then(|name| name.as_str())
                .unwrap_or("")
                .to_string()
        }
        Err(e) => format!("BANDEC_00600: {}", e)
    }
}

#[tauri::command]
fn mudband_ui_enroll(enrollment_token: String, device_name: String, enrollment_secret: Option<String>) -> String {
    let command = serde_json::json!({
        "cmd": "enroll",
        "args": {
            "enrollment_token": enrollment_token,
            "device_name": device_name,
            "enrollment_secret": enrollment_secret
        }
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            serde_json::to_string(&json).unwrap_or_else(|_| "{}".to_string())
        }
        Err(e) => serde_json::to_string(&serde_json::json!({
            "status": 500,
            "msg": e.to_string()
        })).unwrap_or_else(|_| "{}".to_string())
    }
}

fn main() {
    tauri::Builder::default()
        .setup(|app| {
            app.manage(Mutex::new(load_config()));
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            mudband_ui_is_user_tos_agreed,
            mudband_ui_set_user_tos_agreed,
            mudband_ui_get_band_name,
            mudband_ui_get_enrollment_count,
            mudband_ui_enroll,
            mudband_ui_tunnel_is_running,
            mudband_ui_tunnel_disconnect,
            mudband_ui_tunnel_connect
        ])
        .run(tauri::generate_context!())
        .expect("BANDEC_00601: error while running tauri application");
}
