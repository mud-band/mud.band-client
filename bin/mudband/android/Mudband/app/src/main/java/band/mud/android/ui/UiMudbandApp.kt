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

import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.outlined.Info
import androidx.compose.material.icons.outlined.Link
import androidx.compose.material.icons.outlined.List
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material3.DrawerState
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.rememberDrawerState
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.NavigationDrawerItem
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarDuration
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import band.mud.android.R
import band.mud.android.ui.model.MudbandAppViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

enum class MudbandScreen(@StringRes val title: Int) {
    EnrollmentNew(title = R.string.enrollment_new),
    EnrollmentChange(title = R.string.enrollment_change),
    Dashboard(title = R.string.dashboard),
}

@Composable
fun MudbandAppBar(
    viewModel: MudbandAppViewModel,
    currentScreen: MudbandScreen,
    canNavigateBack: Boolean,
    navigateUp: () -> Unit,
    modifier: Modifier = Modifier,
    drawerState: DrawerState
) {
    val scope = rememberCoroutineScope()
    val uiState by viewModel.uiState.collectAsState()

    TopAppBar(
        title = { Text(uiState.topAppBarTitle) },
        modifier = modifier,
        navigationIcon = {
            if (canNavigateBack) {
                IconButton(onClick = navigateUp) {
                    Icon(
                        imageVector = Icons.Filled.ArrowBack,
                        contentDescription = "Back Button"
                    )
                }
            } else {
                IconButton(onClick = {
                    scope.launch {
                        if (drawerState.isClosed) {
                            drawerState.open()
                        } else {
                            drawerState.close()
                        }
                    }
                }) {
                    Icon(Icons.Default.Menu, contentDescription = "Menu")
                }
            }
        }
    )
}

@Composable
fun UiMudbandScaffold(viewModel: MudbandAppViewModel, navController: NavHostController,
                      drawerState: DrawerState
) {
    val backStackEntry by navController.currentBackStackEntryAsState()
    val currentScreen = MudbandScreen.valueOf(
        backStackEntry?.destination?.route ?: MudbandScreen.EnrollmentNew.name
    )
    val snackbarHostState = remember { SnackbarHostState() }
    val coroutineScope = rememberCoroutineScope()

    Scaffold(
        snackbarHost = {
            SnackbarHost(hostState = snackbarHostState)
        },
        topBar = {
            MudbandAppBar(
                viewModel = viewModel,
                currentScreen = currentScreen,
                canNavigateBack = navController.previousBackStackEntry != null,
                navigateUp = { navController.navigateUp() },
                drawerState = drawerState
            )
        }
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = MudbandScreen.Dashboard.name,
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(innerPadding)
        ) {
            composable(route = MudbandScreen.EnrollmentNew.name) {
                UiEnrollmentNewScreen(
                    viewModel = viewModel,
                    navController = navController,
                    onEnrollSuccess = {
                        coroutineScope.launch {
                            withContext(Dispatchers.Main) {
                                viewModel.setEnrollStatus(true)
                                snackbarHostState.showSnackbar(
                                    message = "Enrolled successfully.",
                                    duration = SnackbarDuration.Short
                                )
                            }
                        }
                    },
                    modifier = Modifier
                        .fillMaxSize()
                        .padding()
                )
            }
            composable(route = MudbandScreen.EnrollmentChange.name) {
                UiEnrollmentChangeScreen(
                    viewModel = viewModel,
                    navController = navController,
                    onEnrollChangeSuccess = {
                        coroutineScope.launch {
                            withContext(Dispatchers.Main) {
                                viewModel.setEnrollStatus(true)
                                snackbarHostState.showSnackbar(
                                    message = "Changed successfully.",
                                    duration = SnackbarDuration.Short
                                )
                            }
                        }
                    },
                    modifier = Modifier
                        .fillMaxSize()
                        .padding()
                )
            }
            composable(route = MudbandScreen.Dashboard.name) {
                UiDashboardScreen(
                    viewModel = viewModel,
                    navController = navController,
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(),
                    onUnenrollSuccess = {
                        coroutineScope.launch {
                            withContext(Dispatchers.Main) {
                                snackbarHostState.showSnackbar(
                                    message = "Unenrolled successfully.",
                                    duration = SnackbarDuration.Short
                                )
                            }
                        }
                    },
                    onNotConnected = {
                        coroutineScope.launch {
                            withContext(Dispatchers.Main) {
                                snackbarHostState.showSnackbar(
                                    message = "Not connected.",
                                    duration = SnackbarDuration.Short
                                )
                            }
                        }
                    }
                )
            }
        }
    }
}

@Composable
fun UiMudbandDrawer(viewModel: MudbandAppViewModel, navController: NavHostController,
                    drawerState: DrawerState) {
    val uiState by viewModel.uiState.collectAsState()
    val coroutineScope = rememberCoroutineScope()

    ModalDrawerSheet {
        Column(
            modifier = Modifier.padding(horizontal = 16.dp)
                .verticalScroll(rememberScrollState())
        ) {
            Spacer(Modifier.height(12.dp))
            Text("Mud.band", modifier = Modifier.padding(16.dp), style = MaterialTheme.typography.titleLarge)
            NavigationDrawerItem(
                icon = { Icon(Icons.Outlined.Info, contentDescription = null) },
                label = { Text("Status") },
                selected = false,
                onClick = {
                    viewModel.setDashboardScreenName("status")
                    coroutineScope.launch {
                        drawerState.close()
                    }
                }
            )
            NavigationDrawerItem(
                icon = { Icon(Icons.Outlined.List, contentDescription = null) },
                label = { Text("Devices") },
                selected = false,
                onClick = {
                    viewModel.setDashboardScreenName("devices")
                    coroutineScope.launch {
                        drawerState.close()
                    }
                }
            )
            NavigationDrawerItem(
                icon = { Icon(Icons.Outlined.Link, contentDescription = null) },
                label = { Text("Links") },
                selected = false,
                onClick = {
                    viewModel.setDashboardScreenName("links")
                    coroutineScope.launch {
                        drawerState.close()
                    }
                }
            )
            NavigationDrawerItem(
                label = { Text("Settings") },
                selected = false,
                icon = { Icon(Icons.Outlined.Settings, contentDescription = null) },
                onClick = {
                    viewModel.setDashboardScreenName("settings")
                    coroutineScope.launch {
                        drawerState.close()
                    }
                }
            )
            Text("v0.0.6", modifier = Modifier.padding(16.dp))
        }
    }
}

@Composable
fun UiMudbandApp(viewModel: MudbandAppViewModel = viewModel(),
                 navController: NavHostController = rememberNavController()) {
    val drawerState = rememberDrawerState(initialValue = DrawerValue.Closed)

    ModalNavigationDrawer(
        drawerContent = {
            UiMudbandDrawer(
                viewModel = viewModel,
                navController = navController,
                drawerState = drawerState
            )
        },
        drawerState = drawerState
    ) {
        UiMudbandScaffold(
            viewModel = viewModel,
            navController = navController,
            drawerState = drawerState
        )
    }
}
