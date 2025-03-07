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
 import androidx.compose.foundation.layout.fillMaxSize
 import androidx.compose.foundation.layout.fillMaxWidth
 import androidx.compose.foundation.layout.Row
 import androidx.compose.foundation.layout.padding
 import androidx.compose.foundation.layout.Spacer
 import androidx.compose.foundation.layout.height
 import androidx.compose.foundation.layout.width
 import androidx.compose.foundation.rememberScrollState
 import androidx.compose.foundation.verticalScroll
 import androidx.compose.material.icons.Icons
 import androidx.compose.material.icons.filled.Info
 import androidx.compose.material.icons.filled.Lock
 import androidx.compose.material.icons.filled.Public
 import androidx.compose.material.icons.filled.Security
 import androidx.compose.material.icons.filled.Settings
 import androidx.compose.material3.Button
 import androidx.compose.material3.ButtonDefaults
 import androidx.compose.material3.Card
 import androidx.compose.material3.CardDefaults
 import androidx.compose.material3.ElevatedCard
 import androidx.compose.material3.Icon
 import androidx.compose.material3.ListItem
 import androidx.compose.material3.MaterialTheme
 import androidx.compose.material3.Text
 import androidx.compose.runtime.Composable
 import androidx.compose.ui.Alignment
 import androidx.compose.ui.Modifier
 import androidx.compose.ui.text.font.FontWeight
 import androidx.compose.ui.unit.dp
 import androidx.compose.ui.text.style.TextAlign
 import androidx.compose.foundation.text.ClickableText
 import androidx.compose.ui.text.SpanStyle
 import androidx.compose.ui.text.buildAnnotatedString
 import androidx.compose.ui.text.withStyle
 import androidx.compose.ui.text.style.TextDecoration
 import androidx.compose.ui.platform.LocalUriHandler
 import androidx.lifecycle.viewmodel.compose.viewModel
 import androidx.navigation.compose.rememberNavController
 import band.mud.android.ui.model.MudbandAppViewModel
 import androidx.navigation.NavHostController
 
 @Composable
 fun UiEnrollmentGuideBandTypePublicScreen(
	viewModel: MudbandAppViewModel,
	navController: NavHostController
 ) {    
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(12.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Header Card - Made more compact
        ElevatedCard(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 12.dp),
            elevation = CardDefaults.elevatedCardElevation(defaultElevation = 2.dp)
        ) {
            Row(
                modifier = Modifier.padding(12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Default.Public,
                    contentDescription = "Public Band",
                    tint = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.padding(end = 8.dp)
                )
                
                Text(
                    text = "Band Guide: Public Band Type",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary
                )
            }
        }
        
        // Information Card - Made more compact
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 8.dp),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.7f)
            )
        ) {
            Column(modifier = Modifier.padding(10.dp)) {
                Text(
                    text = "About Public Bands",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold
                )
                
                Text(
                    text = "This band is set to PUBLIC mode. Here's what that means for your security:",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }
        
        // List Items - Made more compact
        Column(modifier = Modifier.fillMaxWidth()) {
            ListItem(
                headlineContent = { 
                    Text(
                        text = "Default blocking policy",
                        style = MaterialTheme.typography.bodyMedium,
                        fontWeight = FontWeight.Medium
                    )
                },
                supportingContent = {
                    Text(
                        "All connection attempts are blocked by default",
                        style = MaterialTheme.typography.bodySmall
                    )
                },
                leadingContent = {
                    Icon(
                        Icons.Default.Lock,
                        contentDescription = "Default Block",
                        tint = MaterialTheme.colorScheme.primary
                    )
                },
                modifier = Modifier.padding(vertical = 2.dp)
            )
            
            ListItem(
                headlineContent = { 
                    Text(
                        text = "Permission-based access",
                        style = MaterialTheme.typography.bodyMedium,
                        fontWeight = FontWeight.Medium
                    )
                },
                supportingContent = {
                    Text(
                        "Nobody can connect without your explicit permission",
                        style = MaterialTheme.typography.bodySmall
                    )
                },
                leadingContent = {
                    Icon(
                        Icons.Default.Security,
                        contentDescription = "Permission Required",
                        tint = MaterialTheme.colorScheme.primary
                    )
                },
                modifier = Modifier.padding(vertical = 2.dp)
            )
            
            ListItem(
                headlineContent = { 
                    Text(
                        text = "ACL configuration required",
                        style = MaterialTheme.typography.bodyMedium,
                        fontWeight = FontWeight.Medium
                    )
                },
                supportingContent = {
                    Text(
                        "You need to add ACL rules to allow specific connections",
                        style = MaterialTheme.typography.bodySmall
                    )
                },
                leadingContent = {
                    Icon(
                        Icons.Default.Settings,
                        contentDescription = "ACL Rules",
                        tint = MaterialTheme.colorScheme.primary
                    )
                },
                modifier = Modifier.padding(vertical = 2.dp)
            )
        }
        
        Spacer(modifier = Modifier.height(8.dp))
        
        // Documentation Card - Made more compact
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 12.dp),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surfaceVariant
            )
        ) {
            Column(
                modifier = Modifier.padding(10.dp)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.padding(bottom = 4.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Info,
                        contentDescription = "Information",
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.padding(end = 8.dp)
                    )
                    
                    Text(
                        text = "How to manage access control",
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold
                    )
                }
                
                Text(
                    text = "To control ACL settings, access the WebCLI interface",
                    style = MaterialTheme.typography.bodySmall,
                    textAlign = TextAlign.Center
                )
                
                val uriHandler = LocalUriHandler.current
                val annotatedString = buildAnnotatedString {
                    append("Learn more at ")
                    
                    pushStringAnnotation(
                        tag = "URL",
                        annotation = "https://mud.band/docs/public-band"
                    )
                    
                    withStyle(
                        style = SpanStyle(
                            color = MaterialTheme.colorScheme.primary,
                            textDecoration = TextDecoration.Underline,
                            fontWeight = FontWeight.Medium
                        )
                    ) {
                        append("mud.band/docs/public-band")
                    }
                }
                
                ClickableText(
                    text = annotatedString,
                    style = MaterialTheme.typography.bodySmall,
                    onClick = { offset ->
                        annotatedString.getStringAnnotations(
                            tag = "URL",
                            start = offset,
                            end = offset
                        ).firstOrNull()?.let { annotation ->
                            uriHandler.openUri(annotation.item)
                        }
                    }
                )
            }
        }
        
        Spacer(modifier = Modifier.weight(1f))
        
        // Continue Button - Slightly more compact
        Button(
            onClick = {
                navController.navigate(MudbandScreen.Dashboard.name) {
                    popUpTo(navController.graph.startDestinationId) {
                        inclusive = true
                    }
                }
            },
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp),
            elevation = ButtonDefaults.buttonElevation(defaultElevation = 2.dp),
            shape = MaterialTheme.shapes.medium
        ) {
            Text(
                text = "Continue to Dashboard",
                style = MaterialTheme.typography.bodyLarge,
                modifier = Modifier.padding(vertical = 2.dp)
            )
        }
    }
 }
