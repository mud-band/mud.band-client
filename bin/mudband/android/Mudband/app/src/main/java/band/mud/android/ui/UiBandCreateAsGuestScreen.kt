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
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.navigation.NavHostController
import band.mud.android.MainApplication
import band.mud.android.ui.model.MudbandAppViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import android.os.Build
import kotlin.random.Random

@Composable
fun UiBandCreateAsGuestScreen(
    viewModel: MudbandAppViewModel,
    navController: NavHostController,
    onBandCreateAsGuestSuccess: () -> Unit,
    onBandCreateAsGuestError: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    val app = MainApplication.applicationContext() as MainApplication
    var bandName by remember { mutableStateOf("") }
    var bandDescription by remember { mutableStateOf("") }
    var isLoading by remember { mutableStateOf(false) }
    var loadingMessage by remember { mutableStateOf("Creating a band...") }
    val coroutineScope = rememberCoroutineScope()
    
    val client = remember { OkHttpClient() }
    
    if (isLoading) {
        Dialog(onDismissRequest = { }) {
            Card(
                modifier = Modifier.padding(16.dp),
                elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
            ) {
                Column(
                    modifier = Modifier
                        .padding(16.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    CircularProgressIndicator()
                    Spacer(modifier = Modifier.height(16.dp))
                    Text(text = loadingMessage)
                }
            }
        }
    }
    
    suspend fun fetchEnrollmentToken(jwt: String): Pair<Int, String> {
        loadingMessage = "Creating an enrollment token..."
        
        return withContext(Dispatchers.IO) {
            try {
                val enrollRequest = Request.Builder()
                    .url("https://www.mud.band/api/band/anonymous/enrollment/token/create")
                    .header("Authorization", jwt)
                    .get()
                    .build()
                
                val enrollResponse = client.newCall(enrollRequest).execute()
                val enrollResponseBody = enrollResponse.body?.string() ?: ""
                
                val jsonResponse = Json.decodeFromString<JsonObject>(enrollResponseBody)
                
                if (!enrollResponse.isSuccessful) {
                    val errorMessage = jsonResponse["msg"]?.jsonPrimitive?.content ?: "Enrollment token error (${enrollResponse.code})"
                    return@withContext Pair(-1, errorMessage)
                }
                
                val token = jsonResponse["token"]?.jsonPrimitive?.content ?: ""
                if (token.isEmpty()) {
                    return@withContext Pair(-1, "Enrollment token error: Token is empty")
                }
                
                return@withContext Pair(jsonResponse["status"]?.jsonPrimitive?.content?.toIntOrNull() ?: -1, token)
            } catch (e: Exception) {
                return@withContext Pair(-1, "Enrollment token error: ${e.message ?: "Unknown error"}")
            }
        }
    }
    
    fun generateRandomString(length: Int): String {
        val charPool = ('a'..'z') + ('A'..'Z') + ('0'..'9')
        return (1..length)
            .map { Random.nextInt(0, charPool.size) }
            .map(charPool::get)
            .joinToString("")
    }
    
    fun getDeviceName(): String {
        val manufacturer = Build.MANUFACTURER
        val model = Build.MODEL
        val randomSuffix = generateRandomString(6)
        
        val deviceName = if (model.startsWith(manufacturer)) {
            "$model-$randomSuffix"
        } else {
            "$manufacturer $model-$randomSuffix"
        }
        return deviceName
    }
    
    suspend fun makeCreateBandRequest(bandName: String, bandDescription: String): Triple<Int, String, String> {
        return withContext(Dispatchers.IO) {
            val jsonObject = buildJsonObject {
                put("name", JsonPrimitive(bandName))
                put("description", JsonPrimitive(bandDescription))
            }
            val requestBody = Json.encodeToString(JsonObject.serializer(), jsonObject)
                .toRequestBody("application/json".toMediaType())
            
            val request = Request.Builder()
                .url("https://www.mud.band/api/band/anonymous/create")
                .post(requestBody)
                .build()
            
            try {
                val response = client.newCall(request).execute()
                val responseBody = response.body?.string() ?: ""
                
                if (!response.isSuccessful) {
                    try {
                        val jsonResponse = Json.decodeFromString<JsonObject>(responseBody)
                        val errorMessage = jsonResponse["message"]?.jsonPrimitive?.content 
                            ?: "Server error occurred (${response.code})"
                        return@withContext Triple(response.code, "", errorMessage)
                    } catch (e: Exception) {
                        return@withContext Triple(response.code, "", "Server error occurred (${response.code})")
                    }
                }
                
                val jsonResponse = Json.decodeFromString<JsonObject>(responseBody)
                val status = jsonResponse["status"]?.jsonPrimitive?.content?.toIntOrNull() ?: -1
                val bandUuid = jsonResponse["band_uuid"]?.jsonPrimitive?.content ?: ""
                val jwt = jsonResponse["jwt"]?.jsonPrimitive?.content ?: ""
                
                Triple(status, bandUuid, jwt)
            } catch (e: Exception) {
                return@withContext Triple(-1, "", "Network error: ${e.message ?: "Unknown error"}")
            }
        }
    }
    
    suspend fun enrollDevice(jwt: String): Pair<Boolean, String> {
        val enrollmentResult = fetchEnrollmentToken(jwt)
        if (enrollmentResult.first == 200 && enrollmentResult.second.isNotEmpty()) {
            val enrollmentToken = enrollmentResult.second
            val deviceName = getDeviceName()
            
            loadingMessage = "Enrolling device..."
            val enrollResult = withContext(Dispatchers.IO) {
                makeEnrollRequest(enrollmentToken, deviceName, "")
            }
            if (enrollResult.status == 0) {
                return Pair(true, "")
            } else {
                return Pair(false, "Band created but enrollment failed: ${enrollResult.msg}")
            }
        } else {
            return Pair(false, "Band created but: ${enrollmentResult.second}")
        }
    }
    
    fun createBand() {
        coroutineScope.launch {
            isLoading = true
            loadingMessage = "Creating a band..."
            try {
                val result = makeCreateBandRequest(bandName, bandDescription)
                
                if (result.first == 200 && result.second.isNotEmpty() && result.third.isNotEmpty()) {
                    val bandUuid = result.second
                    val jwt = result.third
                    
                    if (app.jni.saveBandAdmin(bandUuid, jwt)) {
                        val enrollResult = enrollDevice(jwt)
                        if (enrollResult.first) {
                            navController.popBackStack()
                            viewModel.refreshEnrollStatus()
                            onBandCreateAsGuestSuccess()
                        } else {
                            onBandCreateAsGuestError(enrollResult.second)
                        }
                    } else {
                        onBandCreateAsGuestError("Failed to save band admin")
                    }
                } else {
                    val errorMessage = if (result.third.startsWith("Network error:")) {
                        result.third
                    } else {
                        "Failed to create band: server returned status code ${result.first}"
                    }
                    onBandCreateAsGuestError(errorMessage)
                }
            } catch (e: Exception) {
                val errorMessage = "Error occurred while creating band: ${e.message ?: "Unknown error"}"
                onBandCreateAsGuestError(errorMessage)
            } finally {
                isLoading = false
            }
        }
    }
    
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "Create Band as Guest",
                style = MaterialTheme.typography.headlineMedium,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center,
                modifier = Modifier
                    .padding(vertical = 24.dp)
                    .fillMaxWidth()
            )
            
            Text(
                text = "Band Information",
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.padding(bottom = 16.dp)
            )
            
            OutlinedTextField(
                value = bandName,
                onValueChange = { bandName = it },
                label = { Text("Band Name") },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true
            )
            
            Spacer(modifier = Modifier.height(16.dp))
            
            OutlinedTextField(
                value = bandDescription,
                onValueChange = { bandDescription = it },
                label = { Text("Band Description") },
                modifier = Modifier.fillMaxWidth(),
                minLines = 3,
                maxLines = 5
            )
            Spacer(modifier = Modifier.height(16.dp))
            
            Text(
                text = "Default band type is private and ACL policy is open.",
                style = MaterialTheme.typography.bodyMedium,
                textAlign = TextAlign.Center,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp)
            )
            Spacer(modifier = Modifier.height(32.dp))
            
            Button(
                onClick = { createBand() },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp),
                enabled = !isLoading
            ) {
                Text(
                    text = "Create Band",
                    style = MaterialTheme.typography.titleMedium,
                    modifier = Modifier.padding(vertical = 8.dp)
                )
            }
        }
    }
}
 