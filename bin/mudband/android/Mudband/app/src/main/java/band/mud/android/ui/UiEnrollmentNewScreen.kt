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

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DevicesOther
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import band.mud.android.MainApplication
import band.mud.android.R
import band.mud.android.ui.model.MudbandAppViewModel
import band.mud.android.util.MudbandLog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.*
import kotlinx.serialization.json.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.text.ClickableText
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.foundation.clickable

@Serializable
data class EnrollmentRequestData(
    val token: String,
    val name: String,
    var wireguard_pubkey: String,
    var secret: String
)

@Serializable
data class EnrollmentResponseBandData(
    val opt_public: Int,
    val name: String
)

@Serializable
data class EnrollmentResponseData(
    val status: Int,
    val msg: String? = null,
    val sso_url: String? = null,
    val band: EnrollmentResponseBandData? = null
)

data class EnrollmentResult(
    var status: Int,
    var msg: String,
    var opt_public: Int = 0
)

fun returnEnrollResult(status: Int, msg: String, opt_public: Int = 0): EnrollmentResult {
    if (status != 0) {
        MudbandLog.e(msg)
    }
    return EnrollmentResult(status, msg, opt_public)
}

suspend fun makeEnrollRequest(enrollmentToken: String, deviceName: String,
                              enrollmentSecret: String): EnrollmentResult {
    val app = MainApplication.applicationContext() as MainApplication
    val keys = app.jni.createWireguardKeys()
        ?: return returnEnrollResult(-1, "BANDEC_00163: Failed to create the wireguard keys.")
    val data = EnrollmentRequestData(enrollmentToken, deviceName, keys[0], enrollmentSecret)
    val mediaType = "application/json; charset=utf-8".toMediaType()
    val post = Json.encodeToString(data).toRequestBody(mediaType)
    val client = OkHttpClient()
    val request = Request.Builder()
        .url("https://www.mud.band/api/band/enroll")
        .post(post)
        .build()
    try {
	val response_p = client.newCall(request)
        val response = response_p.execute()
        if (!response.isSuccessful) {
            return returnEnrollResult(-4, "BANDEC_00164: Unexpected status ${response.code}")
        }
        val responseData = response.body?.string()
            ?: return returnEnrollResult(-2, "BANDEC_00165: Failed to get the response body.")
        val jsonWithUnknownKeys = Json { ignoreUnknownKeys = true }
        val obj = jsonWithUnknownKeys.decodeFromString<EnrollmentResponseData>(responseData)
        if (obj.status != 200) {
            if (obj.status == 301) {
                return returnEnrollResult(-10, obj.sso_url ?: "https://mud.band/api/error/BANDEC_00494")
            }
            return returnEnrollResult(-5, obj.msg ?: "BANDEC_00166: msg is null")
        }
        val r = app.jni.parseEnrollmentResponse(keys[1], responseData)
        if (r != 0) {
            return returnEnrollResult(-7, "BANDEC_00167: app.jni.parseEnrollmentResponse() failed.")
        }
        return returnEnrollResult(0, "Okay", obj.band?.opt_public ?: 0)
    } catch (e: Exception) {
        e.printStackTrace()
        return returnEnrollResult(-3, "BANDEC_00168: Exception ${e.message}")
    }
}

@Composable
fun EnrollmentErrorAlertDialog(
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
fun EnrollmentMfaAlertDialog(
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
fun EnrollmentSuccessAlertDialog(
    onConfirmation: () -> Unit,
    isPublic: Boolean
) {
    val uriHandler = LocalUriHandler.current
    val message = if (isPublic) {
        buildAnnotatedString {
            append("NOTE: This band is public.\n\n")
            append("• Nobody can connect to your device without your permission\n")
            append("• Your default policy is 'block'\n")
            append("• You need to add an ACL rule to allow the connection\n")
            append("• To control ACL, you need to open the WebCLI\n\n")
            append("For details, please visit ")
            withStyle(style = SpanStyle(color = MaterialTheme.colorScheme.primary)) {
                pushStringAnnotation(
                    tag = "URL",
                    annotation = "https://mud.band/docs/public-band"
                )
                append("https://mud.band/docs/public-band")
                pop()
            }
        }
    } else {
        buildAnnotatedString {
            append("NOTE: This band is private.\n\n")
            append("• Band admin only can control ACL rules and the default policy\n")
            append("• You can't control your device\n\n")
            append("For details, please visit ")
            withStyle(style = SpanStyle(color = MaterialTheme.colorScheme.primary)) {
                pushStringAnnotation(
                    tag = "URL",
                    annotation = "https://mud.band/docs/private-band"
                )
                append("https://mud.band/docs/private-band")
                pop()
            }
        }
    }

    AlertDialog(
        icon = {
            Icon(Icons.Default.Info, contentDescription = "Success Icon")
        },
        title = {
            Text(text = "Enrollment successful")
        },
        text = {
            ClickableText(
                text = message,
                style = MaterialTheme.typography.bodyMedium.copy(
                    color = MaterialTheme.colorScheme.onSurface
                ),
                onClick = { offset: Int ->
                    message.getStringAnnotations(tag = "URL", start = offset, end = offset)
                        .firstOrNull()?.let { annotation ->
                            uriHandler.openUri(annotation.item)
                        }
                }
            )
        },
        onDismissRequest = {},
        confirmButton = {
            TextButton(
                onClick = onConfirmation
            ) {
                Text("Okay")
            }
        },
        dismissButton = null
    )
}

@Composable
fun UiEnrollmentNewScreen(
    viewModel: MudbandAppViewModel,
    navController: NavHostController,
    onEnrollSuccess: () -> Unit,
    modifier: Modifier = Modifier
) {
    val uriHandler = LocalUriHandler.current

    var enrollmentToken by remember { mutableStateOf("") }
    var deviceName by remember { mutableStateOf("") }
    var enrollmentSecret by remember { mutableStateOf("") }
    val coroutineScope = rememberCoroutineScope()
    val enrollmentErrorAlertDialogOpen = remember { mutableStateOf(false) }
    val enrollmentErrorAlertDialogMessage = remember { mutableStateOf("") }
    val enrollmentMfaAlertDialogOpen = remember { mutableStateOf(false) }
    val enrollmentMfaAlertDialogURL = remember { mutableStateOf("") }
    val enrollmentSuccessDialogOpen = remember { mutableStateOf(false) }
    val enrollmentSuccessPublicValue = remember { mutableStateOf(0) }

    if (enrollmentErrorAlertDialogOpen.value) {
        EnrollmentErrorAlertDialog(
            onDismissRequest = {
                enrollmentErrorAlertDialogOpen.value = false
            },
            dialogTitle = "Enrollment",
            dialogText = enrollmentErrorAlertDialogMessage.value,
            icon = Icons.Default.Info
        )
    }
    
    if (enrollmentMfaAlertDialogOpen.value) {
        EnrollmentMfaAlertDialog(
            onDismissRequest = {
                enrollmentMfaAlertDialogOpen.value = false
            },
            onConfirmation = {
                uriHandler.openUri(enrollmentMfaAlertDialogURL.value)
            },
            dialogTitle = "Enrollment MFA",
            dialogText = "MFA (multi-factor authentication) is enabled to enroll.",
            icon = Icons.Default.Info
        )
    }

    if (enrollmentSuccessDialogOpen.value) {
        EnrollmentSuccessAlertDialog(
            onConfirmation = {
                enrollmentSuccessDialogOpen.value = false
                navController.popBackStack()
                onEnrollSuccess()
            },
            isPublic = enrollmentSuccessPublicValue.value == 1
        )
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = "Device Enrollment",
            style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(bottom = 24.dp)
        )
        
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text(
                text = "Please enter your enrollment information below",
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.padding(bottom = 8.dp)
            )
            
            OutlinedTextField(
                value = enrollmentToken,
                onValueChange = { enrollmentToken = it },
                label = { Text("Enrollment Token") },
                leadingIcon = { 
                    Icon(
                        Icons.Default.Info, 
                        contentDescription = "Token"
                    ) 
                },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true
            )
            
            OutlinedTextField(
                value = deviceName,
                onValueChange = { deviceName = it },
                label = { Text("Device Name") },
                leadingIcon = { 
                    Icon(
                        Icons.Filled.DevicesOther, 
                        contentDescription = "Device"
                    ) 
                },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true
            )
            
            OutlinedTextField(
                value = enrollmentSecret,
                onValueChange = { enrollmentSecret = it },
                label = { Text("Enrollment Secret (optional)") },
                leadingIcon = { 
                    Icon(
                        Icons.Filled.Lock, 
                        contentDescription = "Secret"
                    ) 
                },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                visualTransformation = PasswordVisualTransformation()
            )
        }
        
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(16.dp, Alignment.CenterHorizontally)
        ) {
            OutlinedButton(
                onClick = { navController.navigateUp() },
                modifier = Modifier.weight(1f)
            ) {
                Text("Cancel")
            }
            
            Button(
                onClick = {
                    coroutineScope.launch {
                        withContext(Dispatchers.IO) {
                            var result = makeEnrollRequest(enrollmentToken, deviceName,
                                enrollmentSecret)
                            if (result.status != 0) {
                                if (result.status == -10 /* SSO_URL */) {
                                    enrollmentMfaAlertDialogOpen.value = true
                                    enrollmentMfaAlertDialogURL.value = result.msg
                                } else {
                                    enrollmentErrorAlertDialogOpen.value = true
                                    enrollmentErrorAlertDialogMessage.value = result.msg
                                }
                            } else {
                                enrollmentSuccessPublicValue.value = result.opt_public
                                enrollmentSuccessDialogOpen.value = true
                            }
                        }
                    }
                },
                modifier = Modifier.weight(1f)
            ) {
                Text("Enroll Device")
            }
        }
    }
    
    SideEffect {
        viewModel.setTopAppBarTitle("New Enrollment")
    }
}