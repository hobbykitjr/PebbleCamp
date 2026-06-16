/**
 * Pixel Camp - PebbleKit JS
 *
 * Handles:
 * - Geocoding zip code to lat/lng
 * - Open-Meteo for weather, sunrise/sunset, 3-hour forecast
 * - Sends all data to watch via AppMessage
 */

var settings = {
  zipCode: '17948',   // Near Locust Lake State Park, PA
  displayMode: 3,     // bit0=sun, bit1=hilo (3 = both on)
  showSun: 1,
  showHiLo: 1,
  showSec: 0,
  useCelsius: 0,
  devMode: 0,
  locError: '',       // Empty = OK, otherwise error message
  lastLat: 0,        // Last successfully geocoded coordinates
  lastLng: 0,
  lastPlace: ''
};

// ============================================================================
// HTTP HELPER
// ============================================================================
function xhrRequest(url, type, callback, errorCallback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    if (xhr.status === 200) callback(this.responseText);
    else { console.log('HTTP ' + xhr.status); if (errorCallback) errorCallback(); }
  };
  xhr.onerror = function () {
    console.log('XHR error'); if (errorCallback) errorCallback();
  };
  xhr.open(type, url);
  xhr.send();
}

// ============================================================================
// GEOCODING
// ============================================================================
function isUSZip(str) { return /^\d{5}(-\d{4})?$/.test(str.trim()); }
function isCanadianPostal(str) { return /^[A-Za-z]\d[A-Za-z]\s?\d[A-Za-z]\d$/.test(str.trim()); }
function isUKPostcode(str) { return /^[A-Za-z]{1,2}\d[A-Za-z\d]?\s?\d[A-Za-z]{2}$/.test(str.trim()); }

function geocodeZippopotam(country, code, callback, errorCallback) {
  var url = 'https://api.zippopotam.us/' + country + '/' + code;
  xhrRequest(url, 'GET', function (resp) {
    try {
      var j = JSON.parse(resp);
      if (j.places && j.places.length > 0) {
        callback(parseFloat(j.places[0].latitude), parseFloat(j.places[0].longitude),
                 j.places[0]['place name'] + ', ' + j.places[0]['state abbreviation']);
      } else { geocodeOpenMeteo(code, callback, errorCallback); }
    } catch (e) { geocodeOpenMeteo(code, callback, errorCallback); }
  }, function () { geocodeOpenMeteo(code, callback, errorCallback); });
}

function geocodeZip(zipCode, callback, errorCallback) {
  var q = zipCode.trim();
  if (isUSZip(q)) {
    geocodeZippopotam('us', q, callback, errorCallback);
  } else if (isCanadianPostal(q)) {
    // Zippopotam uses FSA (first 3 chars) for Canada
    var fsa = q.replace(/\s/g, '').substring(0, 3).toUpperCase();
    geocodeZippopotam('ca', fsa, callback, errorCallback);
  } else if (isUKPostcode(q)) {
    // Zippopotam uses outward code (part before space) for UK
    var parts = q.replace(/\s/g, '');
    var outward = parts.substring(0, parts.length - 3).toUpperCase();
    geocodeZippopotam('gb', outward, callback, errorCallback);
  } else {
    geocodeOpenMeteo(q, callback, errorCallback);
  }
}

function geocodeOpenMeteo(query, callback, errorCallback) {
  var url = 'https://geocoding-api.open-meteo.com/v1/search?name=' +
            encodeURIComponent(query) + '&count=1&language=en&format=json';
  xhrRequest(url, 'GET', function (resp) {
    try {
      var j = JSON.parse(resp);
      if (j.results && j.results.length > 0) {
        var r = j.results[0];
        callback(r.latitude, r.longitude, r.name);
      } else {
        // Open-Meteo didn't find it — try Nominatim (handles postal codes)
        geocodeNominatim(query, callback, errorCallback);
      }
    } catch (e) {
      console.log('Geocode error: ' + e);
      geocodeNominatim(query, callback, errorCallback);
    }
  }, function () { geocodeNominatim(query, callback, errorCallback); });
}

function geocodeNominatim(query, callback, errorCallback) {
  var url = 'https://nominatim.openstreetmap.org/search?q=' +
            encodeURIComponent(query) + '&format=json&limit=1';
  xhrRequest(url, 'GET', function (resp) {
    try {
      var j = JSON.parse(resp);
      if (j && j.length > 0) {
        var name = j[0].display_name.split(',')[0];
        callback(parseFloat(j[0].lat), parseFloat(j[0].lon), name);
      } else {
        console.log('Nominatim: no results for ' + query);
        if (errorCallback) errorCallback('Location "' + query + '" not found');
      }
    } catch (e) {
      console.log('Nominatim error: ' + e);
      if (errorCallback) errorCallback('Location lookup failed');
    }
  }, function () {
    if (errorCallback) errorCallback('Location lookup failed (network error)');
  });
}

// ============================================================================
// WEATHER CODES
// ============================================================================
function wmoToSimple(wmo) {
  if (wmo === 0) return 0;          // Clear
  if (wmo <= 2) return 1;           // Cloudy
  if (wmo === 3) return 2;          // Overcast
  if (wmo <= 48) return 3;          // Fog
  if (wmo <= 67) return 4;          // Rain
  if (wmo <= 77) return 6;          // Snow
  if (wmo <= 86) return 6;          // Snow showers
  if (wmo >= 95) return 5;          // Thunderstorm
  return 1;
}

// ============================================================================
// WEATHER + SUN + FORECAST (Open-Meteo combined call)
// ============================================================================
function fetchWeatherAndForecast(lat, lng, useCelsius, callback) {
  var unitStr = useCelsius ? 'celsius' : 'fahrenheit';
  console.log('Weather fetch: unit=' + unitStr + ' (useCelsius=' + useCelsius + ')');
  var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + lat +
            '&longitude=' + lng +
            '&current=temperature_2m,weather_code,wind_speed_10m' +
            '&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min' +
            '&hourly=weather_code,temperature_2m' +
            '&temperature_unit=' + unitStr +
            '&wind_speed_unit=mph' +
            '&timezone=auto' +
            '&forecast_days=2';

  xhrRequest(url, 'GET', function (resp) {
    try {
      var j = JSON.parse(resp);
      var temp = Math.round(j.current.temperature_2m);
      var wxCode = wmoToSimple(j.current.weather_code);
      var wind = j.current.wind_speed_10m;
      if (wind > 20 && wxCode < 4) wxCode = 7;

      var srParts = j.daily.sunrise[0].split('T')[1].split(':');
      var ssParts = j.daily.sunset[0].split('T')[1].split(':');
      var hi = Math.round(j.daily.temperature_2m_max[0]);
      var lo = Math.round(j.daily.temperature_2m_min[0]);

      // Smart peek slots: pick 3 meaningful future times
      var now = new Date();
      var curHour = now.getHours();
      var sunsetH = parseInt(ssParts[0]);

      // Slot 1: afternoon/next daytime block (2-3pm or next meaningful hour)
      // Slot 2: evening/night
      // Slot 3: tomorrow midday
      var slots = [];
      if (curHour < 12) {
        slots.push(14);  // This afternoon
        slots.push(sunsetH + 1);  // Tonight
        slots.push(24 + 12);  // Tomorrow noon
      } else if (curHour < sunsetH) {
        slots.push(sunsetH + 1);  // Tonight
        slots.push(24 + 8);  // Tomorrow morning
        slots.push(24 + 14);  // Tomorrow afternoon
      } else {
        slots.push(24 + 8);   // Tomorrow morning
        slots.push(24 + 14);  // Tomorrow afternoon
        slots.push(24 + sunsetH + 1);  // Tomorrow night
      }

      var peeks = [];
      for (var s = 0; s < 3; s++) {
        var idx = slots[s];
        if (idx < j.hourly.weather_code.length) {
          var hTime = j.hourly.time[idx];
          peeks.push({
            hour: parseInt(hTime.split('T')[1].split(':')[0]),
            wx: wmoToSimple(j.hourly.weather_code[idx]),
            temp: Math.round(j.hourly.temperature_2m[idx])
          });
        } else {
          peeks.push({ hour: 12, wx: 0, temp: 0 });
        }
      }

      console.log('Weather: ' + temp + 'F, hi=' + hi + ' lo=' + lo + ', wx=' + wxCode);
      console.log('Sun: ' + srParts[0] + ':' + srParts[1] + ' - ' + ssParts[0] + ':' + ssParts[1]);
      console.log('Peeks: ' + JSON.stringify(peeks));

      callback({
        temperature: temp, weatherCode: wxCode,
        tempHigh: hi, tempLow: lo,
        sunriseHour: parseInt(srParts[0]), sunriseMin: parseInt(srParts[1]),
        sunsetHour: parseInt(ssParts[0]), sunsetMin: parseInt(ssParts[1]),
        peeks: peeks
      });
    } catch (e) {
      console.log('Weather parse error: ' + e);
      callback(null);
    }
  }, function () { callback(null); });
}

// ============================================================================
// DATA FETCH
// ============================================================================
var fetchGeneration = 0;

function sendWeather(lat, lng, placeName, useCelsius, myGen) {
  fetchWeatherAndForecast(lat, lng, useCelsius, function (data) {
    if (!data) return;
    if (myGen !== fetchGeneration) { console.log('Fetch gen ' + myGen + ' superseded'); return; }
    var msg = {
      'SUNRISE_HOUR': data.sunriseHour,
      'SUNRISE_MIN': data.sunriseMin,
      'SUNSET_HOUR': data.sunsetHour,
      'SUNSET_MIN': data.sunsetMin,
      'TEMPERATURE': data.temperature,
      'WEATHER_CODE': data.weatherCode,
      'TEMP_HIGH': data.tempHigh,
      'TEMP_LOW': data.tempLow,
      'TOWN_NAME': placeName || '',
      'PEEK_WX1': data.peeks[0].wx,  'PEEK_T1': data.peeks[0].temp,  'PEEK_H1': data.peeks[0].hour,
      'PEEK_WX2': data.peeks[1].wx,  'PEEK_T2': data.peeks[1].temp,  'PEEK_H2': data.peeks[1].hour,
      'PEEK_WX3': data.peeks[2].wx,  'PEEK_T3': data.peeks[2].temp,  'PEEK_H3': data.peeks[2].hour
    };
    Pebble.sendAppMessage(msg,
      function () { console.log('Data sent (gen ' + myGen + ')'); },
      function () { console.log('Send failed (gen ' + myGen + ')'); });
  });
}

function fetchAllData() {
  var myGen = ++fetchGeneration;
  // Capture settings at call time so async callbacks use correct values
  var useCelsius = settings.useCelsius;
  console.log('Fetching data for: ' + settings.zipCode + ' (celsius=' + useCelsius + ' gen=' + myGen + ')');
  geocodeZip(settings.zipCode, function (lat, lng, placeName) {
    if (myGen !== fetchGeneration) { console.log('Fetch gen ' + myGen + ' superseded'); return; }
    // Geocoding succeeded — cache coordinates and clear error
    settings.lastLat = lat;
    settings.lastLng = lng;
    settings.lastPlace = placeName;
    settings.locError = '';
    saveSettings();
    sendWeather(lat, lng, placeName, useCelsius, myGen);
  }, function (errMsg) {
    if (myGen !== fetchGeneration) return;
    console.log('Geocode failed: ' + errMsg + ', using cached coords');
    // Geocoding failed — but if we have cached coordinates, still fetch weather
    // (this ensures unit changes take effect even if geocoding is flaky)
    if (settings.lastLat && settings.lastLng) {
      sendWeather(settings.lastLat, settings.lastLng, settings.lastPlace, useCelsius, myGen);
    } else {
      settings.locError = errMsg;
      saveSettings();
    }
  });
}

// ============================================================================
// SETTINGS
// ============================================================================
function loadSettings() {
  try {
    var s = localStorage.getItem('settings');
    if (s) {
      var p = JSON.parse(s);
      if (p.zipCode) settings.zipCode = p.zipCode;
      if (p.displayMode !== undefined) settings.displayMode = p.displayMode;
      if (p.showSun !== undefined) settings.showSun = p.showSun;
      if (p.showHiLo !== undefined) settings.showHiLo = p.showHiLo;
      if (p.showSec !== undefined) settings.showSec = p.showSec;
      if (p.useCelsius !== undefined) settings.useCelsius = p.useCelsius;
      if (p.devMode !== undefined) settings.devMode = p.devMode;
      if (p.locError !== undefined) settings.locError = p.locError;
      if (p.lastLat !== undefined) settings.lastLat = p.lastLat;
      if (p.lastLng !== undefined) settings.lastLng = p.lastLng;
      if (p.lastPlace !== undefined) settings.lastPlace = p.lastPlace;
    }
  } catch (e) {}
}
function saveSettings() {
  localStorage.setItem('settings', JSON.stringify(settings));
}

Pebble.addEventListener('showConfiguration', function () {
  var url = 'https://hobbykitjr.github.io/PebbleCamp/config/index.html' +
    '?zip=' + encodeURIComponent(settings.zipCode) +
    '&unit=' + (settings.useCelsius !== undefined ? settings.useCelsius : 0) +
    '&sun=' + (settings.showSun !== undefined ? settings.showSun : 1) +
    '&hilo=' + (settings.showHiLo !== undefined ? settings.showHiLo : 1) +
    '&sec=' + (settings.showSec !== undefined ? settings.showSec : 0) +
    '&dev=' + settings.devMode +
    '&locError=' + encodeURIComponent(settings.locError || '');
  console.log('Opening config: ' + url);
  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (e && e.response && e.response.length > 0) {
    try {
      var rawResponse = e.response;
      if (rawResponse.indexOf('response=') === 0) rawResponse = rawResponse.substring(9);
      // Try decoding; if already decoded, try parsing directly
      var decoded;
      try { decoded = decodeURIComponent(rawResponse); } catch(de) { decoded = rawResponse; }
      var config = JSON.parse(decoded);
      console.log('Config parsed OK: celsius=' + config.useCelsius + ' zip=' + config.zipCode);
      if (config.zipCode) settings.zipCode = config.zipCode;
      if (config.displayMode !== undefined) settings.displayMode = parseInt(config.displayMode);
      if (config.showSun !== undefined) settings.showSun = parseInt(config.showSun);
      if (config.showHiLo !== undefined) settings.showHiLo = parseInt(config.showHiLo);
      if (config.showSec !== undefined) settings.showSec = parseInt(config.showSec);
      if (config.useCelsius !== undefined) settings.useCelsius = parseInt(config.useCelsius);
      if (config.devMode !== undefined) settings.devMode = parseInt(config.devMode);
      saveSettings();
      console.log('Settings updated: zip=' + settings.zipCode + ' celsius=' + settings.useCelsius);
      Pebble.sendAppMessage({'DISPLAY_MODE': settings.displayMode, 'DEV_MODE': settings.devMode},
        function() { console.log('Settings sent, fetching weather'); fetchAllData(); },
        function() { console.log('Settings send failed, fetching anyway'); fetchAllData(); });
    } catch (err) {
      console.log('Config parse error: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function () {
  console.log('PebbleKit JS ready');
  loadSettings();
  // Delay fetch slightly to let webviewclosed fire first if config just closed
  // (webviewclosed has the authoritative new settings)
  setTimeout(function() {
    // Re-load in case webviewclosed updated settings while we waited
    loadSettings();
    fetchAllData();
  }, 500);
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload['REQUEST_DATA']) fetchAllData();
});
