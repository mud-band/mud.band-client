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
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
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
import androidx.compose.ui.res.dimensionResource
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

@Serializable
data class EnrollmentRequestData(
    val token: String,
    val name: String,
    var wireguard_pubkey: String
)

@Serializable
data class EnrollmentResponseData(
    val status: Int,
    val msg: String? = null
)

data class EnrollmentResult(
    var status: Int,
    var msg: String
)

fun returnEnrollResult(status: Int, msg: String): EnrollmentResult {
    if (status != 0) {
        MudbandLog.e(msg)
    }
    return EnrollmentResult(status, msg)
}

suspend fun makeEnrollRequest(enrollmentToken: String, deviceName: String): EnrollmentResult {
    val app = MainApplication.applicationContext() as MainApplication
    val keys = app.jni.createWireguardKeys()
        ?: return returnEnrollResult(-1, "BANDEC_00163: Failed to create the wireguard keys.")
    val data = EnrollmentRequestData(enrollmentToken, deviceName, keys[0])
    val mediaType = "application/json; charset=utf-8".toMediaType()
    val post = Json.encodeToString(data).toRequestBody(mediaType)
    val client = OkHttpClient()
    val request = Request.Builder()
        .url("https://www.mud.band/api/band/enroll")
        .post(post)
        .build()
    try {
        val response = client.newCall(request).execute()
        if (!response.isSuccessful) {
            return returnEnrollResult(-4, "BANDEC_00164: Unexpected status ${response.code}")
        }
        val responseData = response.body?.string()
            ?: return returnEnrollResult(-2, "BANDEC_00165: Failed to get the response body.")
        val jsonWithUnknownKeys = Json { ignoreUnknownKeys = true }
        val obj = jsonWithUnknownKeys.decodeFromString<EnrollmentResponseData>(responseData)
        if (obj.status != 200) {
            return returnEnrollResult(-5, obj.msg ?: "BANDEC_00166: msg is null")
        }
        val r = app.jni.parseEnrollmentResponse(keys[1], responseData)
        if (r != 0) {
            return returnEnrollResult(-7, "BANDEC_00167: app.jni.parseEnrollmentResponse() failed.")
        }
        return returnEnrollResult(0, "Okay")
    } catch (e: Exception) {
        e.printStackTrace()
        return returnEnrollResult(-3, "BANDEC_00168: Exception ${e.message}")
    }
}

@Composable
fun EnrollmentAlertDialog(
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
fun UiEnrollmentNewScreen(
    viewModel: MudbandAppViewModel,
    navController: NavHostController,
    onEnrollSuccess: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
    ) {
        Column(
            modifier = Modifier.fillMaxWidth()
        ) {
            var enrollmentToken by remember { mutableStateOf("") }
            var deviceName by remember { mutableStateOf("") }
            val coroutineScope = rememberCoroutineScope()
            val alertDialogOpen = remember { mutableStateOf(false) }
            val alertDialogMessage = remember { mutableStateOf("") }

            if (alertDialogOpen.value) {
                EnrollmentAlertDialog(
                    onDismissRequest = {
                        alertDialogOpen.value = false
                    },
                    dialogTitle = "Enrollment",
                    dialogText = alertDialogMessage.value,
                    icon = Icons.Default.Info
                )
            }

            Text(
                text = "Enrollment",
                modifier = modifier
                    .padding(dimensionResource(R.dimen.padding_small))
            )
            TextField(
                value = enrollmentToken,
                onValueChange = { enrollmentToken = it },
                label = { Text("Enrollment Token") },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(dimensionResource(R.dimen.padding_small))
            )
            TextField(
                value = deviceName,
                onValueChange = { deviceName = it },
                label = { Text("Device Name") },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(dimensionResource(R.dimen.padding_small))
            )
            Column(
                modifier = modifier,
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Row {
                    Button(
                        onClick = {
                            coroutineScope.launch {
                                withContext(Dispatchers.IO) {
                                    var result = makeEnrollRequest(enrollmentToken, deviceName)
                                    if (result.status != 0) {
                                        alertDialogOpen.value = true
                                        alertDialogMessage.value = result.msg
                                    } else {
                                        withContext(Dispatchers.Main) {
                                            navController.popBackStack()
                                            onEnrollSuccess()
                                        }
                                    }
                                }
                            }
                        }
                    ) {
                        Text("Enroll")
                    }
                    Spacer(modifier = Modifier.width(8.dp))
                    Button(onClick = { navController.navigateUp() }) {
                        Text("Back")
                    }
                }
            }
        }
    }
    SideEffect {
        viewModel.setTopAppBarTitle("New Enrollment")
    }
}