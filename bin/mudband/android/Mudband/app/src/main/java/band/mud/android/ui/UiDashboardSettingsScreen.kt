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

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material.icons.rounded.Clear
import androidx.compose.material.icons.rounded.Create
import androidx.compose.material.icons.rounded.KeyboardArrowRight
import androidx.compose.material.icons.rounded.List
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.Divider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.navigation.NavHostController
import band.mud.android.MainApplication
import band.mud.android.R
import band.mud.android.ui.model.MudbandAppViewModel
import band.mud.android.util.MudbandLog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.Json
import okhttp3.OkHttpClient
import okhttp3.Request

/*
 * These are from the following URLs:
 *
 * - https://tomas-repcik.medium.com/making-extensible-settings-screen-in-jetpack-compose-from-scratch-2558170dd24d
 */

@Composable
fun SettingsSwitchComp(
    @DrawableRes icon: Int,
    @StringRes iconDesc: Int,
    @StringRes name: Int,
    state: State<Boolean>,
    onClick: () -> Unit
) {
    Surface(
        color = Color.Transparent,
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
        onClick = onClick,
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    painterResource(id = icon),
                    contentDescription = stringResource(id = iconDesc),
                    modifier = Modifier.size(24.dp),
                    tint = MaterialTheme.colorScheme.primary
                )
                Spacer(modifier = Modifier.width(16.dp))
                Text(
                    text = stringResource(id = name),
                    style = MaterialTheme.typography.bodyLarge,
                    textAlign = TextAlign.Start,
                )
            }
            Spacer(modifier = Modifier.weight(1f))
            Switch(
                checked = state.value,
                onCheckedChange = { onClick() }
            )
        }
    }
}

@Composable
private fun TextEditDialog(
    @StringRes name: Int,
    storedValue: State<String>,
    onSave: (String) -> Unit,
    onCheck: (String) -> Boolean,
    onDismiss: () -> Unit // internal method to dismiss dialog from within
) {

    // storage for new input
    var currentInput by remember {
        mutableStateOf(TextFieldValue(storedValue.value))
    }

    // if the input is valid - run the method for current value
    var isValid by remember {
        mutableStateOf(onCheck(storedValue.value))
    }

    Surface(
        color = MaterialTheme.colorScheme.surfaceTint
    ) {

        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
        ) {
            Text(stringResource(id = name))
            Spacer(modifier = Modifier.height(8.dp))
            TextField(currentInput, onValueChange = {
                // check on change, if the value is valid
                isValid = onCheck(it.text)
                currentInput = it
            })
            Row {
                Spacer(modifier = Modifier.weight(1f))
                Button(onClick = {
                    // save and dismiss the dialog
                    onSave(currentInput.text)
                    onDismiss()
                    // disable / enable the button
                }, enabled = isValid) {
                    Text("Next")
                }
            }
        }
    }
}

@Composable
fun SettingsTextComp(
    @DrawableRes icon: Int,
    @StringRes iconDesc: Int,
    @StringRes name: Int,
    state: State<String>,
    onSave: (String) -> Unit,
    onCheck: (String) -> Boolean
) {
    // if the dialog is visible
    var isDialogShown by remember { mutableStateOf(false) }

    // conditional visibility in dependence to state
    if (isDialogShown) {
        Dialog(onDismissRequest = {
            // dismiss the dialog on touch outside
            isDialogShown = false
        }) {
            TextEditDialog(name, state, onSave, onCheck) {
                // to dismiss dialog from within
                isDialogShown = false
            }
        }
    }

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
        onClick = {
            isDialogShown = true
        },
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, 
                modifier = Modifier.weight(1f)) {
                Icon(
                    painterResource(id = icon),
                    contentDescription = stringResource(id = iconDesc),
                    modifier = Modifier.size(24.dp),
                    tint = MaterialTheme.colorScheme.primary
                )
                Spacer(modifier = Modifier.width(16.dp))
                Column {
                    Text(
                        text = stringResource(id = name),
                        style = MaterialTheme.typography.bodyLarge,
                        textAlign = TextAlign.Start,
                    )
                    Text(
                        text = state.value,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        textAlign = TextAlign.Start,
                    )
                }
            }
            Icon(
                Icons.Rounded.KeyboardArrowRight,
                contentDescription = stringResource(id = R.string.ic_arrow_forward),
                tint = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
fun SettingsClickableComp(
    icon: ImageVector,
    @StringRes iconDesc: Int,
    @StringRes name: Int,
    onClick: () -> Unit
) {
    Surface(
        color = Color.Transparent,
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        onClick = onClick,
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 16.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.weight(1f)
            ) {
                Icon(
                    icon,
                    contentDescription = stringResource(id = iconDesc),
                    modifier = Modifier.size(24.dp),
                    tint = MaterialTheme.colorScheme.primary
                )
                Spacer(modifier = Modifier.width(16.dp))
                Text(
                    text = stringResource(id = name),
                    style = MaterialTheme.typography.bodyLarge,
                    textAlign = TextAlign.Start,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            Icon(
                Icons.Rounded.KeyboardArrowRight,
                contentDescription = stringResource(id = R.string.ic_arrow_forward),
                tint = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
fun SettingsGroup(
    @StringRes name: Int,
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit
) {
    Column(modifier = modifier.padding(horizontal = 16.dp, vertical = 8.dp)) {
        Text(
            text = stringResource(id = name),
            style = MaterialTheme.typography.titleMedium,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.padding(start = 8.dp, bottom = 8.dp)
        )
        Surface(
            modifier = modifier.fillMaxWidth(),
            shape = RoundedCornerShape(12.dp),
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f),
            tonalElevation = 1.dp
        ) {
            Column(modifier = Modifier.padding(vertical = 4.dp)) {
                content()
            }
        }
    }
}

data class UnenrollmentResult(
    var status: Int,
    var msg: String
)

fun returnUnenrollResult(status: Int, msg: String): UnenrollmentResult {
    if (status != 0) {
        MudbandLog.e(msg)
    }
    return UnenrollmentResult(status, msg)
}

suspend fun makeUnenrollRequest(): UnenrollmentResult {
    val app = MainApplication.applicationContext() as MainApplication
    val client = OkHttpClient()
    val request = Request.Builder()
        .url("https://www.mud.band/api/band/unenroll")
        .addHeader("Authorization", app.jni.getBandJWT())
        .build()
    try {
        val response = client.newCall(request).execute()
        if (!response.isSuccessful) {
            return returnUnenrollResult(-4, "BANDEC_00170: Unexpected status ${response.code}")
        }
        val responseData = response.body?.string()
            ?: return returnUnenrollResult(-2, "BANDEC_00171: Failed to get the response body.")
        val jsonWithUnknownKeys = Json { ignoreUnknownKeys = true }
        val obj = jsonWithUnknownKeys.decodeFromString<EnrollmentResponseData>(responseData)
        if (obj.status != 200) {
            if (obj.status == 505 /* No band found */ ||
                obj.status == 506 /* No device found */) {
                /* do nothing and fallthrough. */
            } else {
                return returnUnenrollResult(-5, obj.msg ?: "BANDEC_00172: msg is null")
            }
        }
        val r = app.jni.parseUnenrollmentResponse(responseData)
        if (r != 0) {
            return returnUnenrollResult(-7, "BANDEC_00173: app.jni.parseUnenrollmentResponse() failed.")
        }
        return returnUnenrollResult(0, "Okay")
    } catch (e: Exception) {
        e.printStackTrace()
        return returnUnenrollResult(-3, "BANDEC_00174: Exception ${e.message}")
    }
}

@Composable
fun UnenrollmentAlertDialog(
    viewModel: MudbandAppViewModel,
    navController: NavHostController,
    onDismissRequest: () -> Unit,
    dialogTitle: String,
    dialogText: String,
    icon: ImageVector,
    onUnenrollSuccess: () -> Unit
) {
    val coroutineScope = rememberCoroutineScope()
    var isLoading by remember { mutableStateOf(false) }

    AlertDialog(
        icon = {
            Icon(icon, contentDescription = null, tint = MaterialTheme.colorScheme.error)
        },
        title = {
            Text(text = dialogTitle, style = MaterialTheme.typography.headlineSmall)
        },
        text = {
            Text(text = dialogText, style = MaterialTheme.typography.bodyMedium)
        },
        onDismissRequest = {
            if (!isLoading) onDismissRequest()
        },
        confirmButton = {
            TextButton(
                onClick = {
                    isLoading = true
                    coroutineScope.launch {
                        withContext(Dispatchers.IO) {
                            val result = makeUnenrollRequest()
                            if (result.status == 0) {
                                withContext(Dispatchers.Main) {
                                    viewModel.refreshEnrollStatus()
                                    navController.navigateUp()
                                    onUnenrollSuccess()
                                }
                            }
                            isLoading = false
                        }
                    }
                },
                enabled = !isLoading
            ) {
                Text("Unenroll", color = MaterialTheme.colorScheme.error)
            }
        },
        dismissButton = {
            TextButton(
                onClick = {
                    if (!isLoading) onDismissRequest()
                },
                enabled = !isLoading
            ) {
                Text("Cancel")
            }
        }
    )
}

@Composable
fun UiDashboardSettingsScreen(
    viewModel: MudbandAppViewModel,
    navController: NavHostController,
    modifier: Modifier = Modifier,
    onUnenrollSuccess: () -> Unit
) {
    val unenrollDialogOpen = remember { mutableStateOf(false) }

    if (unenrollDialogOpen.value) {
        UnenrollmentAlertDialog(
            viewModel = viewModel,
            navController = navController,
            onDismissRequest = {
                unenrollDialogOpen.value = false
            },
            dialogTitle = "Unenrollment",
            dialogText = "Do you really want to unenroll? You can't revert after unenrollment.",
            icon = Icons.Default.Warning,
            onUnenrollSuccess = onUnenrollSuccess
        )
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 16.dp)
    ) {
        SettingsGroup(
            name = R.string.band,
            modifier = modifier
        ) {
            SettingsClickableComp(
                name = R.string.band_create_as_guest,
                icon = Icons.Rounded.Create,
                iconDesc = R.string.band_create_as_guest,
            ) {
                navController.navigate(MudbandScreen.BandCreateAsGuest.name)
            }
        }
        
        Spacer(modifier = Modifier.height(8.dp))
        
        SettingsGroup(
            name = R.string.enrollment,
            modifier = modifier
        ) {
            SettingsClickableComp(
                name = R.string.enrollment_new,
                icon = Icons.Rounded.Add,
                iconDesc = R.string.enrollment_new,
            ) {
                navController.navigate(MudbandScreen.EnrollmentNew.name)
            }
            Divider(
                modifier = Modifier.padding(horizontal = 16.dp),
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.1f)
            )
            SettingsClickableComp(
                name = R.string.enrollment_change,
                icon = Icons.Rounded.List,
                iconDesc = R.string.enrollment_new,
            ) {
                navController.navigate(MudbandScreen.EnrollmentChange.name)
            }
        }
        
        Spacer(modifier = Modifier.height(16.dp))
        
        SettingsGroup(
            name = R.string.danger_zone,
            modifier = modifier
        ) {
            SettingsClickableComp(
                name = R.string.unenroll,
                icon = Icons.Rounded.Clear,
                iconDesc = R.string.unenroll,
            ) {
                unenrollDialogOpen.value = true
            }
        }
    }
    SideEffect {
        viewModel.setTopAppBarTitle("Settings")
    }
}
