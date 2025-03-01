#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::sync::Mutex;
use std::fs;
use std::io::{Write, BufRead};
use serde::{Deserialize, Serialize};
use tauri::Manager;
use std::io::BufReader;
use windows_named_pipe::PipeStream;
use windows::Win32::UI::Shell::{FOLDERID_ProgramData, SHGetKnownFolderPath};

#[derive(Default, Serialize, Deserialize)]
struct AppState {
    #[serde(default = "default_user_tos_agreed")]
    user_tos_agreed: bool,
}

fn default_user_tos_agreed() -> bool {
    false
}

fn get_config_path() -> std::path::PathBuf {
    let program_data = unsafe {
        let path_ptr = SHGetKnownFolderPath(&FOLDERID_ProgramData, windows::Win32::UI::Shell::KF_FLAG_DEFAULT, None)
            .expect("Failed to get ProgramData path");
        let path = path_ptr.to_string().unwrap();
        path
    };

    let mut config_dir = std::path::PathBuf::from(program_data);
    config_dir.push("Mudfish Networks");
    config_dir.push("Mud.band");
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
    let mut client = PipeStream::connect(r"\\.\pipe\mudband_service")
        .map_err(|e| format!("Failed to connect to pipe: {}", e))?;

    let command_str = serde_json::to_string(&command)
        .map_err(|e| format!("Failed to serialize command: {}", e))? + "\n";

    client.write_all(command_str.as_bytes())
        .map_err(|e| format!("Failed to write to pipe: {}", e))?;
    client.flush()
        .map_err(|e| format!("Failed to flush pipe: {}", e))?;

    let mut response = String::new();
    let mut reader = BufReader::new(client);
    
    reader.read_line(&mut response)
        .map_err(|e| format!("Failed to read from pipe: {}", e))?;

    serde_json::from_str(&response.trim())
        .map_err(|e| format!("Failed to parse response: {}", e))
}

#[tauri::command]
fn mudband_ui_tunnel_connect(_state: tauri::State<'_, Mutex<AppState>>) -> i64 {
    let command = serde_json::json!({
        "cmd": "tunnel_connect"
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            json.get("status")
                .and_then(|s| s.as_u64())
                .map(|s| s as i64)
                .unwrap_or(500)
        }
        Err(e) => {
            println!("BANDEC_00708: {}", e);
            500
        }
    }
}

#[tauri::command]
fn mudband_ui_tunnel_disconnect(_state: tauri::State<'_, Mutex<AppState>>) -> i64 {
    let command = serde_json::json!({
        "cmd": "tunnel_disconnect"
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            json.get("status")
                .and_then(|s| s.as_u64())
                .map(|s| s as i64)
                .unwrap_or(500)
        }
        Err(e) => {
            println!("BANDEC_00709: {}", e);
            500
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
                println!("BANDEC_00710: Invalid status code");
                return false;
            }
            json.get("tunnel_is_running")
                .and_then(|status| status.as_bool())
                .unwrap_or(false)
        }
        Err(e) => {
            println!("BANDEC_00711: {}", e);
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
                println!("BANDEC_00712: Invalid status code");
                return 0;
            }
            json.get("enrollment_count")
                .and_then(|count| count.as_i64())
                .unwrap_or(0)
        }
        Err(e) => {
            println!("BANDEC_00713: {}", e);
            -1
        }
    }
}

#[tauri::command]
fn mudband_ui_get_active_band(_state: tauri::State<'_, Mutex<AppState>>) -> String {
    let command = serde_json::json!({
        "cmd": "get_active_band",
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

#[tauri::command]
fn mudband_ui_get_band_admin(_state: tauri::State<'_, Mutex<AppState>>) -> String {
    let command = serde_json::json!({
        "cmd": "get_band_admin"
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

#[tauri::command]
fn mudband_ui_save_band_admin(band_uuid: String, jwt: String) -> String {
    let command = serde_json::json!({
        "cmd": "save_band_admin",
        "args": {
            "band_uuid": band_uuid,
            "jwt": jwt
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

#[tauri::command]
fn mudband_ui_get_active_conf(_state: tauri::State<'_, Mutex<AppState>>) -> String {
    let command = serde_json::json!({
        "cmd": "get_active_conf"
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            serde_json::to_string(&json).unwrap_or_else(|_| "{}".to_string())
        }
        Err(e) => serde_json::to_string(&serde_json::json!({
            "status": 500,
            "msg": format!("BANDEC_00714: {}", e)
        })).unwrap_or_else(|_| "{}".to_string())
    }
}

#[tauri::command]
fn mudband_ui_get_enrollment_list(_state: tauri::State<'_, Mutex<AppState>>) -> String {
    let command = serde_json::json!({
        "cmd": "get_enrollment_list"
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            serde_json::to_string(&json).unwrap_or_else(|_| "[]".to_string())
        }
        Err(e) => serde_json::to_string(&serde_json::json!({
            "status": 500,
            "msg": format!("BANDEC_00715: {}", e)
        })).unwrap_or_else(|_| "[]".to_string())
    }
}

#[tauri::command]
fn mudband_ui_unenroll(_state: tauri::State<'_, Mutex<AppState>>, band_uuid: String) -> String {
    let command = serde_json::json!({
        "cmd": "unenroll",
        "args": {
            "band_uuid": band_uuid
        }
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            serde_json::to_string(&json).unwrap_or_else(|_| "{}".to_string())
        }
        Err(e) => serde_json::to_string(&serde_json::json!({
            "status": 500,
            "msg": format!("BANDEC_00716: {}", e)
        })).unwrap_or_else(|_| "{}".to_string())
    }
}

#[tauri::command]
fn mudband_ui_change_enrollment(_state: tauri::State<'_, Mutex<AppState>>, band_uuid: String) -> String {
    let command = serde_json::json!({
        "cmd": "change_enrollment",
        "args": {
            "band_uuid": band_uuid
        }
    });

    match mudband_ui_ipc_send(command) {
        Ok(json) => {
            serde_json::to_string(&json).unwrap_or_else(|_| "{}".to_string())
        }
        Err(e) => serde_json::to_string(&serde_json::json!({
            "status": 500,
            "msg": format!("BANDEC_00717: {}", e)
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
            mudband_ui_get_active_band,
            mudband_ui_get_enrollment_count,
            mudband_ui_enroll,
            mudband_ui_tunnel_is_running,
            mudband_ui_tunnel_disconnect,
            mudband_ui_tunnel_connect,
            mudband_ui_get_active_conf,
            mudband_ui_get_enrollment_list,
            mudband_ui_change_enrollment,
            mudband_ui_unenroll,
            mudband_ui_get_band_admin,
            mudband_ui_save_band_admin
        ])
        .run(tauri::generate_context!())
        .expect("BANDEC_00718: error while running tauri application");
}
