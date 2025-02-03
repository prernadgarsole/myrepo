# Prompt for credentials (for local testing, remove for CyberArk)
$username = Read-Host "Enter your Emp ID"
$password = (Read-Host "Enter your Password").Trim()
$cred = "$($username):$($password)"

$encodedCreds = [System.Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes($cred))
$basicAuthValue = "Basic $encodedCreds"
$Headers = @{Authorization = $basicAuthValue}

# API Endpoints
$priorWeekUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=priorWeek"
$UCD_Report = New-Item -ItemType File -Path .\UCD_Prod_Report.csv -Force
Add-Content $UCD_Report -Value "Application Name,Environment Name,User Name,Start Time,End Time,Component Name,Component Version"

# Fetch report
try {
    Invoke-WebRequest -Uri "$priorWeekUrl" -Method Get -Headers $Headers -OutFile tempReport.json
} catch {
    Write-Host "Error fetching report: $_"
    exit 1  # Exit if API fails
}

$reportJson = Get-Content 'tempReport.json' | Out-String | ConvertFrom-Json

foreach ($item in $reportJson.items) {
    $env = $item.environment
    $appRequestID = $item.applicationRequestId
    $status = $item.status

    if ($status -eq "SUCCESS") {
        $AppRequestUrl = "https://udeploy.nnnn.net:8443/cli/applicationProcessRequest/info/$appRequestID"

        try {
            Invoke-WebRequest -Uri "$AppRequestUrl" -Method Get -Headers $Headers -OutFile tempAppRequest.json
        } catch {
            Write-Host "Error fetching app request: $_"
            continue  # Skip this iteration if request fails
        }

        $requestJson = Get-Content 'tempAppRequest.json' | Out-String | ConvertFrom-Json
        $AppID = $requestJson.application.id
        $AppName = $requestJson.application.name
        $EnvName = $requestJson.environment.name
        $userName = $requestJson.userName

        # Convert UNIX timestamps to readable format
        $startTime = (get-date "1/1/1970").AddSeconds([int64][math]::Truncate([double]$requestJson.startTime / 1000)).ToLocalTime()
        $endTime = (get-date "1/1/1970").AddSeconds([int64][math]::Truncate([double]$requestJson.endTime / 1000)).ToLocalTime()

        $line = "$AppName,$EnvName,$userName,$startTime,$endTime"

        # Production Environments (Fetch Snapshot Versions)
        if ($env -like "PRD*" -or $env -like "PROD*") {
            $SnapshotUrl = "https://udeploy.nnnn.net:8443/cli/snapshot/getSnapshotVersions?application=$AppID&snapshot=$requestJson.snapshot.id"

            try {
                Invoke-WebRequest -Uri "$SnapshotUrl" -Method Get -Headers $Headers -OutFile tempSnapshot.json
                $snapshotJson = Get-Content 'tempSnapshot.json' | Out-String | ConvertFrom-Json

                foreach ($snap in $snapshotJson) {
                    if ($snap.name -and $snap.desiredVersions.name) {
                        $line += ",$($snap.name),$($snap.desiredVersions.name)"
                    }
                }
            } catch {
                Write-Host "Error fetching snapshot versions: $_"
            }
        }
        # Non-Production Environments (Fetch Manifest Versions)
        else {
            $ManifestUrl = "https://udeploy.nnnn.net:8443/cli/applicationProcessRequest/requestManifest/$appRequestID"

            try {
                Invoke-WebRequest -Uri "$ManifestUrl" -Method Get -Headers $Headers -OutFile tempManifest.json
                $manifestJson = Get-Content 'tempManifest.json' | Out-String | ConvertFrom-Json

                foreach ($comp in $manifestJson.components) {
                    if ($comp.name -and $comp.version) {
                        $line += ",$($comp.name),$($comp.version)"
                    }
                }
            } catch {
                Write-Host "Error fetching manifest versions: $_"
            }
        }

        Write-Output "$line"
        Add-Content $UCD_Report -Value "$line"
    }
}
