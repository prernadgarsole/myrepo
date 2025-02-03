# Function to handle web request errors and log them
function Invoke-WebRequestWithErrorHandling {
    param (
        [string]$url,
        [string]$method,
        [hashtable]$headers,
        [string]$outputFile
    )
    
    try {
        $response = Invoke-WebRequest -Uri "$($url)" -Method $method -Headers $headers -OutFile $outputFile -ErrorAction Stop
        return $response
    } catch {
        $errorMessage = $_.Exception.Message
        $statusCode = $_.Exception.Response.StatusCode
        $statusDescription = $_.Exception.Response.StatusDescription
        Write-Host "Error during request to $url. Status Code: $statusCode - $statusDescription. Message: $errorMessage"
        return $null
    }
}

# Prompt for credentials
$username = Read-Host "Enter your Emp ID"
$password = (Read-Host "Enter your Password").Trim()

$cred = "$($username):$($password)"

$priorWeekUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=priorWeek&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"
$priorMonthUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=priorMonth&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"
$currentMonthUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=currentMonth&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"

$encodedCreds = [System.Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes($cred))
$basicAuthValue = "Basic $encodedCreds"
$Headers = @{Authorization = $basicAuthValue}

# Create the report file
$UCD_Report = New-Item -ItemType File -Path .\UCD_Prod_Report.csv -Force
Add-Content $UCD_Report -Value "Application Name,Environment Name,User Name,Start Time,End Time,Component Name,Component Version"

# Function to process report data
function Process-Report($url) {
    $response = Invoke-WebRequestWithErrorHandling -url $url -method "Get" -headers $Headers -outputFile "tempReport.json"
    if ($response -eq $null) {
        Write-Host "Skipping report processing due to request failure for URL: $url"
        return
    }

    $reportJson = Get-Content 'tempReport.json' | Out-String | ConvertFrom-Json
    $reportJson.items | foreach {
        $items = $_
        $items | foreach {
            $item = $_
            $env = $_.environment
            $appRequestID = $_.applicationRequestId
            $status = $_.status
            if ($status -eq "SUCCESS")
            {
                $AppRequestUrl = "https://udeploy.nnnn.net:8443/cli/applicationProcessRequest/info/$appRequestID"
                $appResponse = Invoke-WebRequestWithErrorHandling -url $AppRequestUrl -method "Get" -headers $Headers -outputFile "tempAppRequest.json"
                if ($appResponse -eq $null) {
                    Write-Host "Skipping application request details for appRequestID: $appRequestID"
                    return
                }

                $requestJson = Get-Content 'tempAppRequest.json' | Out-String | ConvertFrom-Json
                $AppID = $requestJson.application.id
                $AppName = $requestJson.application.name
                $SnapshotID = $requestJson.snapshot.id
                $SnapshotName = $requestJson.snapshot.name
                $EnvName = $requestJson.environment.name
                $userName = $requestJson.userName
                $startTimeUnix = $requestJson.startTime
                $unixTime = [int64][math]::Truncate([double]$startTimeUnix / 1000)
                $tempDate = get-date "1/1/1970"
                $startTime = $tempDate.AddSeconds($unixTime).ToLocalTime()
                $endTimeUnix = $requestJson.endTime
                $unixTime = [int64][math]::Truncate([double]$endTimeUnix / 1000)
                $tempDate = get-date "1/1/1970"
                $endTime = $tempDate.AddSeconds($unixTime).ToLocalTime()
                $line = $AppName
                $line = $line + "," + $EnvName
                $line = $line + "," + $userName
                $line = $line + "," + $startTime
                $line = $line + "," + $endTime

                if (($env -like "PRD*") -or ($env -like "PROD*"))
                {
                    $SnapshotUrl = "https://udeploy.nnnn.net:8443/cli/snapshot/getSnapshotVersions?application=$AppID&snapshot=$SnapshotID"
                    $snapshotResponse = Invoke-WebRequestWithErrorHandling -url $SnapshotUrl -method "Get" -headers $Headers -outputFile "tempSnapshot.json"
                    if ($snapshotResponse -eq $null) {
                        Write-Host "Skipping snapshot details for SnapshotID: $SnapshotID"
                        return
                    }

                    $snapshotJson = Get-Content 'tempSnapshot.json' | Out-String | ConvertFrom-Json
                    $snapshotJson | foreach {
                        $compName=$_.name
                        $version=$_.desiredVersions.name
                        if ($version -ne $null)
                        {
                            $line = $line + "," + $compName
                            $line = $line + "," + $version
                        }
                    }
                } else {
                    # Logic to retrieve manifest for non-production deployments
                    if ($env -in @("UNIT", "UNIT2", "INTG", "INTG2", "PERF", "PRE-PROD", "PRDD", "PRDW")) {
                        $ManifestUrl = "https://udeploy.nnnn.net:8443/cli/applicationProcessRequest/requestManifest/$appRequestID"
                        $manifestResponse = Invoke-WebRequestWithErrorHandling -url $ManifestUrl -method "Get" -headers $Headers -outputFile "tempManifest.json"
                        if ($manifestResponse -eq $null) {
                            Write-Host "Skipping manifest retrieval for appRequestID: $appRequestID"
                            return
                        }

                        $manifestJson = Get-Content 'tempManifest.json' | Out-String | ConvertFrom-Json
                        $manifestJson.components | foreach {
                            $compName = $_.name
                            $compVersion = $_.version
                            if ($compName -and $compVersion) {
                                $line = $line + "," + $compName
                                $line = $line + "," + $compVersion
                            }
                        }
                    }
                }

                Write-Output "$line"
                Add-Content $UCD_Report -Value "$line"
            }
        }
    }
}

# Process all reports (priorWeek, priorMonth, currentMonth)
Process-Report $priorWeekUrl
Process-Report $priorMonthUrl
Process-Report $currentMonthUrl
