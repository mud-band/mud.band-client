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
import android.os.Handler
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Error
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
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
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import band.mud.android.MainActivity
import band.mud.android.ui.model.MudbandAppViewModel
import band.mud.android.util.MudbandLog
import band.mud.android.vpn.MudbandVpnResultReceiver

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
fun UiDashboardStatusScreen(
    viewModel: MudbandAppViewModel,
    navController: NavHostController,
    modifier: Modifier = Modifier
) {
    val uiState by viewModel.uiState.collectAsState()
    val context = LocalContext.current
    val uriHandler = LocalUriHandler.current
    var connectButtonEnabled by remember { mutableStateOf(true) }
    val connectErrorAlertDialogOpen = remember { mutableStateOf(false) }
    var connectErrorAlertDialogMessage = remember { mutableStateOf("") }
    val connectMfaAlertDialogOpen = remember { mutableStateOf(false) }
    var connectMfaAlertDialogURL = remember { mutableStateOf("") }

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
                    MudbandLog.e("BANDEC_XXXX: MFA required but no SSO_URL found.")
                }
            }

            else -> {
                MudbandLog.e("BANDEC_00162: Unexpected result code: $resultCode")
            }
        }
    }

    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        if (connectErrorAlertDialogOpen.value) {
            DashboardStatusConnectErrorAlertDialog(
                onDismissRequest = {
                    connectErrorAlertDialogOpen.value = false
                    connectButtonEnabled = true
                },
                dialogTitle = "Status",
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
                dialogTitle = "MFA",
                dialogText = "MFA (multi-factor authentication) is required to connect.",
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
                dialogTitle = "MFA",
                dialogText = "MFA (multi-factor authentication) is required to re-activate the tunnel.",
                icon = Icons.Default.Error
            )
        }
        Text(text = "Band name: ${uiState.activeBandName}")
        if (uiState.activeDeviceName.isNotEmpty()) {
            Text(text = "Device name: ${uiState.activeDeviceName}")
        }
        if (uiState.activePrivateIP.isNotEmpty()) {
            Text(text = "Private IP: ${uiState.activePrivateIP}")
        }
        Spacer(Modifier.height(12.dp))
        if (uiState.isConnected) {
            Button(
                onClick = {
                    var act = context.getActivity() as MainActivity
                    act.disconnectVpnService(resultReceiver)
                },
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.secondary
                )
            ) {
                Row(
                    modifier = Modifier.padding(8.dp)
                ) {
                    Icon(Icons.Filled.PlayArrow, "Disconnect")
                    Text(text = "Disconnect")
                }
            }
        } else {
            Button(
                onClick = {
                    var act = context.getActivity() as MainActivity
                    act.prepareAndConnectVpnService(resultReceiver)
                    connectButtonEnabled = false
                },
                enabled = connectButtonEnabled,
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.secondary
                )
            ) {
                Row(
                    modifier = Modifier.padding(8.dp)
                ) {
                    Icon(Icons.Filled.PlayArrow, "Connect")
                    Text(text = "Connect")
                }
            }
        }
    }
    SideEffect {
        viewModel.setTopAppBarTitle("Mud.band")
    }
}