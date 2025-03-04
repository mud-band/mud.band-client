/*-
 * Copyright (c) 2024 Weongyo Jeong <weongyo@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

package band.mud.android.ui

import android.content.Context
import android.content.ContextWrapper
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.content.ClipboardManager
import android.content.ClipData
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Error
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Divider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import band.mud.android.MainActivity
import band.mud.android.MainApplication
import band.mud.android.ui.model.MudbandAppViewModel
import band.mud.android.util.MudbandLog
import band.mud.android.vpn.MudbandVpnResultReceiver
import band.mud.android.JniWrapper
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request

fun Context.getActivity(): ComponentActivity? {
    var currentContext = this
    while (currentContext is ContextWrapper) {
        if (currentContext is ComponentActivity) {
            return currentContext
        }
        currentContext = currentContext.baseContext
    }
    return null
}

@Composable
fun DashboardStatusConnectErrorAlertDialog(
    onDismissRequest: () -> Unit,
    dialogTitle: String,
    dialogText: String,
    icon: ImageVector,
) {
    AlertDialog(
        icon = {
            Icon(icon, contentDescription = "Example Icon")
        },
        title = {
            Text(text = dialogTitle)
        },
        text = {
            Text(text = dialogText)
        },
        onDismissRequest = {
            onDismissRequest()
        },
        confirmButton = {},
        dismissButton = {
            TextButton(
                onClick = {
                    onDismissRequest()
                }
            ) {
                Text("Dismiss")
            }
        }
    )
}

@Composable
fun DashboardStatusMfaErrorAlertDialog(
    onDismissRequest: () -> Unit,
    onConfirmation: () -> Unit,
    dialogTitle: String,
    dialogText: String,
    icon: ImageVector,
) {
    AlertDialog(
        icon = {
            Icon(icon, contentDescription = "Example Icon")
        },
        title = {
            Text(text = dialogTitle)
        },
        text = {
            Text(text = dialogText)
        },
        onDismissRequest = {
            onDismissRequest()
        },
        confirmButton = {
            TextButton(
                onClick = {
                    onConfirmation()
                }
            ) {
                Text("Open URL")
            }
        },
        dismissButton = {
            TextButton(
                onClick = {
                    onDismissRequest()
                }
            ) {
                Text("Dismiss")
            }
        }
    )
}

@Composable
fun EnrollmentTokenDialog(
    token: String,
    onDismissRequest: () -> Unit,
    onCopyToClipboard: (String) -> Unit
) {
    AlertDialog(
        title = {
            Text("Enrollment Token")
        },
        text = {
            Column {
                Text("Share this token with users to invite them to your band:")
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = token,
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Bold
                )
            }
        },
        onDismissRequest = onDismissRequest,
        confirmButton = {
            TextButton(
                onClick = { onCopyToClipboard(token) }
            ) {
                Text("Copy to Clipboard")
            }
        },
        dismissButton = {
            TextButton(
                onClick = onDismissRequest
            ) {
                Text("Close")
            }
        }
    )
}

@Composable
fun UiDashboardStatusScreen(
    viewModel: MudbandAppViewModel,
    navController: NavHostController,
    modifier: Modifier = Modifier
) {
    val app = MainApplication.applicationContext() as MainApplication
    val uiState by viewModel.uiState.collectAsState()
    val context = LocalContext.current
    val uriHandler = LocalUriHandler.current
    var connectButtonEnabled by remember { mutableStateOf(true) }
    val connectErrorAlertDialogOpen = remember { mutableStateOf(false) }
    var connectErrorAlertDialogMessage = remember { mutableStateOf("") }
    val connectMfaAlertDialogOpen = remember { mutableStateOf(false) }
    var connectMfaAlertDialogURL = remember { mutableStateOf("") }
    val coroutineScope = rememberCoroutineScope()
    
    val bandAdminJson = remember { app.jni.getBandAdmin() }
    val isBandAdmin = remember { bandAdminJson != null }
    val bandAdminUuid = remember { 
        if (bandAdminJson != null) {
            try {
                val jsonObject = Json.decodeFromString<JsonObject>(bandAdminJson)
                jsonObject["band_uuid"]?.jsonPrimitive?.content
            } catch (e: Exception) {
                null
            }
        } else {
            null
        }
    }
    val bandAdminJwt = remember { 
        if (bandAdminJson != null) {
            try {
                val jsonObject = Json.decodeFromString<JsonObject>(bandAdminJson)
                jsonObject["jwt"]?.jsonPrimitive?.content
            } catch (e: Exception) {
                null
            }
        } else {
            null
        }
    }

    val resultReceiver = MudbandVpnResultReceiver(Handler(Looper.getMainLooper())) { resultCode, data ->
        when (resultCode) {
            200 -> {
                viewModel.setConnectStatus(true)
            }

            201 -> {
                viewModel.setConnectStatus(false)
                connectButtonEnabled = true
            }

            202 -> {
                /* This code means that VPN service is connecting.  Nothing to do at this moment. */
            }

            in 300..399 -> {
                MudbandLog.e("BANDEC_00429: VPN connection error: $resultCode")
                connectErrorAlertDialogOpen.value = true
                if (data != null) {
                    connectErrorAlertDialogMessage.value = data.getString("msg").toString()
                } else {
                    connectErrorAlertDialogMessage.value = "VPN connection error"
                }
            }

            401 -> {
                connectMfaAlertDialogOpen.value = true
                if (data != null) {
                    connectMfaAlertDialogURL.value = data.getString("msg").toString()
                } else {
                    MudbandLog.e("BANDEC_00808: MFA required but no SSO_URL found.")
                }
            }

            else -> {
                MudbandLog.e("BANDEC_00162: Unexpected result code: $resultCode")
            }
        }
    }

    // Add state for enrollment token dialog
    var showEnrollmentTokenDialog by remember { mutableStateOf(false) }
    var enrollmentToken by remember { mutableStateOf("") }
    var isLoadingToken by remember { mutableStateOf(false) }
    var tokenError by remember { mutableStateOf<String?>(null) }
    
    // Function to fetch enrollment token
    fun fetchEnrollmentToken() {
        if (bandAdminJwt == null || bandAdminUuid == null) {
            tokenError = "Missing admin credentials"
            return
        }
        
        isLoadingToken = true
        tokenError = null
        
        // Launch in a coroutine
        coroutineScope.launch(Dispatchers.IO) {
            try {
                val client = OkHttpClient()
                val request = Request.Builder()
                    .url("https://www.mud.band/api/band/anonymous/enrollment/token")
                    .addHeader("Authorization", bandAdminJwt)
                    .build()
                
                client.newCall(request).execute().use { response ->
                    val responseBody = response.body?.string() ?: ""
                    
                    withContext(Dispatchers.Main) {
                        isLoadingToken = false
                        
                        if (response.isSuccessful) {
                            try {
                                val jsonObject = Json.decodeFromString<JsonObject>(responseBody)
                                val status = jsonObject["status"]?.jsonPrimitive?.content?.toInt()
                                val tokenUuid = jsonObject["token_uuid"]?.jsonPrimitive?.content
                                
                                if (status == 200 && !tokenUuid.isNullOrEmpty()) {
                                    enrollmentToken = tokenUuid
                                    showEnrollmentTokenDialog = true
                                } else {
                                    tokenError = "Invalid response format"
                                }
                            } catch (e: Exception) {
                                tokenError = "Failed to parse response: ${e.message}"
                                MudbandLog.e("BANDEC_00832: Failed to parse enrollment token response: ${e.message}")
                            }
                        } else {
                            tokenError = "Server returned ${response.code}: $responseBody"
                            MudbandLog.e("BANDEC_00833: Failed to get enrollment token: ${response.code}")
                        }
                    }
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    isLoadingToken = false
                    tokenError = "Network error: ${e.message}"
                    MudbandLog.e("BANDEC_00834: Network error getting enrollment token: ${e.message}")
                }
            }
        }
    }

    Box(
        modifier = modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // Only show Admin Controls card if user is a band admin
            if (isBandAdmin) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    elevation = CardDefaults.cardElevation(defaultElevation = 4.dp),
                    shape = RoundedCornerShape(12.dp)
                ) {
                    Column(
                        modifier = Modifier.padding(16.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Text(
                            text = "Admin Controls",
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.Bold
                        )
                        
                        Text(
                            text = "You are the admin of this band.",
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        
                        Divider(modifier = Modifier.padding(vertical = 8.dp))
                        
                        Button(
                            onClick = { fetchEnrollmentToken() },
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(8.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.secondary
                            ),
                            enabled = !isLoadingToken
                        ) {
                            if (isLoadingToken) {
                                CircularProgressIndicator(
                                    modifier = Modifier.size(24.dp),
                                    color = MaterialTheme.colorScheme.onSecondary,
                                    strokeWidth = 2.dp
                                )
                                Spacer(modifier = Modifier.padding(horizontal = 8.dp))
                            }
                            Text(
                                text = if (isLoadingToken) "Getting token..." else "Get enrollment token",
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                        
                        if (tokenError != null) {
                            Text(
                                text = tokenError!!,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.error,
                                modifier = Modifier.padding(top = 4.dp)
                            )
                        }
                    }
                }
            }

            Card(
                modifier = Modifier.fillMaxWidth(),
                elevation = CardDefaults.cardElevation(defaultElevation = 4.dp),
                shape = RoundedCornerShape(12.dp)
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "Connection Status",
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.Bold
                        )
                        
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Box(
                                modifier = Modifier
                                    .size(12.dp)
                                    .clip(CircleShape)
                                    .background(
                                        if (uiState.isConnected) Color.Green else Color.Red
                                    )
                            )
                            Spacer(modifier = Modifier.padding(4.dp))
                            Text(
                                text = if (uiState.isConnected) "Connected" else "Disconnected",
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                    }
                    
                    Divider(modifier = Modifier.padding(vertical = 8.dp))
                    
                    Text(
                        text = "Band: ${uiState.activeBandName}",
                        style = MaterialTheme.typography.bodyMedium,
                        fontWeight = FontWeight.Medium
                    )
                    
                    if (uiState.activeDeviceName.isNotEmpty()) {
                        Text(
                            text = "Device: ${uiState.activeDeviceName}",
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                    
                    if (uiState.activePrivateIP.isNotEmpty()) {
                        Text(
                            text = "Private IP: ${uiState.activePrivateIP}",
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
            }
        }

        // Float the button at the bottom of the screen
        Button(
            onClick = {
                val act = context.getActivity() as MainActivity
                if (uiState.isConnected) {
                    act.disconnectVpnService(resultReceiver)
                } else {
                    act.prepareAndConnectVpnService(resultReceiver)
                    connectButtonEnabled = false
                }
            },
            enabled = connectButtonEnabled || uiState.isConnected,
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth(0.8f)
                .height(56.dp)
                .padding(bottom = 16.dp),
            shape = RoundedCornerShape(28.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = if (uiState.isConnected) 
                    MaterialTheme.colorScheme.error 
                else 
                    MaterialTheme.colorScheme.primary
            )
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.Center
            ) {
                Icon(
                    if (uiState.isConnected) Icons.Filled.Stop else Icons.Filled.PlayArrow,
                    contentDescription = if (uiState.isConnected) "Disconnect" else "Connect"
                )
                Spacer(modifier = Modifier.padding(horizontal = 8.dp))
                Text(
                    text = if (uiState.isConnected) "Disconnect" else "Connect",
                    style = MaterialTheme.typography.titleMedium
                )
            }
        }
    }

    if (connectErrorAlertDialogOpen.value) {
        DashboardStatusConnectErrorAlertDialog(
            onDismissRequest = {
                connectErrorAlertDialogOpen.value = false
                connectButtonEnabled = true
            },
            dialogTitle = "Connection Error",
            dialogText = connectErrorAlertDialogMessage.value,
            icon = Icons.Default.Error
        )
    }
    
    if (connectMfaAlertDialogOpen.value) {
        DashboardStatusMfaErrorAlertDialog(
            onDismissRequest = {
                connectMfaAlertDialogOpen.value = false
                connectButtonEnabled = true
            },
            onConfirmation = {
                uriHandler.openUri(connectMfaAlertDialogURL.value)
            },
            dialogTitle = "MFA Required",
            dialogText = "Multi-factor authentication is required to connect.",
            icon = Icons.Default.Error
        )
    }
    
    if (uiState.mfaRequired) {
        DashboardStatusMfaErrorAlertDialog(
            onDismissRequest = {
                connectMfaAlertDialogOpen.value = false
                connectButtonEnabled = true
            },
            onConfirmation = {
                uriHandler.openUri(uiState.mfaUrl)
                viewModel.resetMfaStatus()
            },
            dialogTitle = "MFA Required",
            dialogText = "Multi-factor authentication is required to re-activate the tunnel.",
            icon = Icons.Default.Error
        )
    }

    // Show the enrollment token dialog when needed
    if (showEnrollmentTokenDialog) {
        EnrollmentTokenDialog(
            token = enrollmentToken,
            onDismissRequest = { showEnrollmentTokenDialog = false },
            onCopyToClipboard = { token ->
                val clipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val clipData = ClipData.newPlainText("Enrollment Token", token)
                clipboardManager.setPrimaryClip(clipData)
                
                // Show toast on Android versions that don't automatically notify
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.S_V2) {
                    Toast.makeText(context, "Token copied to clipboard", Toast.LENGTH_SHORT).show()
                }
            }
        )
    }

    SideEffect {
        viewModel.setTopAppBarTitle("Mud.band")
    }
}