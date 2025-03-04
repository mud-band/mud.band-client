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

import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.NavHostController
import band.mud.android.ui.model.MudbandAppViewModel

@Composable
fun UiDashboardScreen(
    viewModel: MudbandAppViewModel,
    navController: NavHostController,
    modifier: Modifier = Modifier,
    onUnenrollSuccess: () -> Unit
) {
    val uiState by viewModel.uiState.collectAsState()

    if (!uiState.isUserTosAgreed) {
        UiDashboardUserTosAgreement(
            viewModel = viewModel,
            navController = navController,
            modifier = modifier
        )
    } else if (uiState.isEnrolled) {
        if (uiState.dashboardScreenName == "status") {
            UiDashboardStatusScreen(
                viewModel = viewModel,
                navController = navController,
                modifier = modifier
            )
        } else if (uiState.dashboardScreenName == "devices") {
            UiDashboardDevicesScreen(
                viewModel = viewModel,
                navController = navController
            )
        } else if (uiState.dashboardScreenName == "links") {
            UiDashboardLinksScreen(
                viewModel = viewModel,
                navController = navController
            )
        } else if (uiState.dashboardScreenName == "settings") {
            UiDashboardSettingsScreen(
                viewModel = viewModel,
                navController = navController,
                onUnenrollSuccess = onUnenrollSuccess
            )
        } else {
            Text("BANDEC_00169: Unexpected screen name.")
        }
    } else {
        UiDashboardNotEnrolledScreen(
            viewModel = viewModel,
            navController = navController,
            modifier = modifier
        )
    }
}