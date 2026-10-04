const SPREADSHEET_ID = "1GtzAwOyUnm99xAUif8FNMzGb7i4DUNFqCUFVNVoyeIM";
const API_KEY = "ESP32_BIOMEDICAL_2026";

/*
  Google Apps Script for the ESP32 Biomedical IoT dashboard.

  Sheets expected (headers in row 1):
    VitalSigns: Timestamp | Heart Rate | SpO2 | RR (ms)
    ECG:        Timestamp | Sample | ECG

  This is the ONE canonical version of this script. There was
  previously a second doPost floating around that wrote a 5-column
  layout (separate Date + Time columns) instead of a single
  Timestamp column. If your VitalSigns sheet currently has 5
  columns, delete those old rows (or move them aside) and make sure
  row 1 reads exactly: Timestamp | Heart Rate | SpO2 | RR (ms)

  The ESP32 sends heart_rate, spo2 and rr_ms.
  ECG support is included for dashboards that already have an ECG sheet.

  IMPORTANT: after pasting this in, go to
  Deploy > Manage deployments > (pencil/edit icon) > Version: New version > Deploy
  Just saving the script does NOT update the live /exec URL.
*/

function doGet(e) {
  if (e && e.parameter && e.parameter.api === "data") {
    return jsonResponse(getDashboardData());
  }

  return HtmlService
    .createHtmlOutputFromFile("Index")
    .setTitle("ESP32 Biomedical Monitor")
    .setXFrameOptionsMode(HtmlService.XFrameOptionsMode.ALLOWALL);
}

function doPost(e) {
  try {
    if (!e || !e.postData || !e.postData.contents) {
      return jsonResponse({
        status: "error",
        message: "No POST data"
      });
    }

    const data = JSON.parse(e.postData.contents);

    if (data.api_key !== API_KEY) {
      return jsonResponse({
        status: "error",
        message: "Invalid API key"
      });
    }

    const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
    const vitalSheet = ss.getSheetByName("VitalSigns");
    const ecgSheet = ss.getSheetByName("ECG");

    if (!vitalSheet) {
      throw new Error('Sheet "VitalSigns" was not found.');
    }

    if (data.heart_rate !== undefined && data.spo2 !== undefined) {
      // Single Timestamp column — do NOT split into separate Date/Time
      // columns, or getDashboardData() below will misread every field.
      const timestamp = data.timestamp ? new Date(data.timestamp) : new Date();

      vitalSheet.appendRow([
        timestamp,
        data.heart_rate,
        data.spo2,
        data.rr_ms || ""
      ]);
    }

    if (ecgSheet && Array.isArray(data.ecg) && data.ecg.length > 0) {
      const startTime = Number(data.ecg_start_ms);
      const sampleRate = Number(data.sample_rate);

      if (Number.isFinite(startTime) && Number.isFinite(sampleRate) && sampleRate > 0) {
        const rows = [];

        for (let i = 0; i < data.ecg.length; i++) {
          const sampleTime = startTime + (i * 1000 / sampleRate);

          rows.push([
            new Date(sampleTime),
            i,
            data.ecg[i]
          ]);
        }

        const startRow = ecgSheet.getLastRow() + 1;

        ecgSheet
          .getRange(startRow, 1, rows.length, 3)
          .setValues(rows);
      }
    }

    return jsonResponse({
      status: "success",
      message: "Data saved",
      ecg_samples: Array.isArray(data.ecg) ? data.ecg.length : 0
    });

  } catch (error) {
    return jsonResponse({
      status: "error",
      message: error.toString()
    });
  }
}

function getDashboardData() {
  const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
  const vitalSheet = ss.getSheetByName("VitalSigns");
  const ecgSheet = ss.getSheetByName("ECG");

  const result = {
    generatedAt: new Date().toISOString(),
    vitals: [],
    ecg: []
  };

  if (vitalSheet) {
    const values = vitalSheet.getDataRange().getValues();

    // Skip the header row. Keep the most recent 200 vital records.
    for (let i = Math.max(1, values.length - 200); i < values.length; i++) {
      const row = values[i];

      if (row.length < 4 || !row[0]) continue;

      const heartRate = Number(row[1]);
      const spo2 = Number(row[2]);
      const rr = Number(row[3]);

      result.vitals.push({
        timestamp: toISOStringSafe(row[0]),
        heartRate: Number.isFinite(heartRate) ? heartRate : null,
        spo2: Number.isFinite(spo2) ? spo2 : null,
        rr: Number.isFinite(rr) ? rr : null
      });
    }
  }

  // If an ECG sheet exists, send only the newest 1000 samples.
  if (ecgSheet) {
    const values = ecgSheet.getDataRange().getValues();

    for (let i = Math.max(1, values.length - 1000); i < values.length; i++) {
      const row = values[i];

      if (row.length < 3 || !row[0]) continue;

      const ecg = Number(row[2]);

      if (Number.isFinite(ecg)) {
        result.ecg.push({
          timestamp: toISOStringSafe(row[0]),
          sample: Number(row[1]) || 0,
          value: ecg
        });
      }
    }
  }

  return result;
}

function toISOStringSafe(value) {
  if (value instanceof Date && !isNaN(value.getTime())) {
    return value.toISOString();
  }

  const parsed = new Date(value);

  if (!isNaN(parsed.getTime())) {
    return parsed.toISOString();
  }

  return String(value);
}

function jsonResponse(object) {
  return ContentService
    .createTextOutput(JSON.stringify(object))
    .setMimeType(ContentService.MimeType.JSON);
}
