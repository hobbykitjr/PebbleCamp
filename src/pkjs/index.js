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
  displayMode: 1,
  devMode: 0
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

function geocodeZip(zipCode, callback) {
  if (isUSZip(zipCode)) {
    var url = 'https://api.zippopotam.us/us/' + zipCode.trim();
    xhrRequest(url, 'GET', function (resp) {
      try {
        var j = JSON.parse(resp);
        callback(parseFloat(j.places[0].latitude), parseFloat(j.places[0].longitude),
                 j.places[0]['place name'] + ', ' + j.places[0]['state abbreviation']);
      } catch (e) { geocodeOpenMeteo(zipCode, callback); }
    }, function () { geocodeOpenMeteo(zipCode, callback); });
  } else {
    geocodeOpenMeteo(zipCode, callback);
  }
}

function geocodeOpenMeteo(query, callback) {
  var url = 'https://geocoding-api.open-meteo.com/v1/search?name=' +
            encodeURIComponent(query) + '&count=1&language=en&format=json';
  xhrRequest(url, 'GET', function (resp) {
    try {
      var j = JSON.parse(resp);
      if (j.results && j.results.length > 0) {
        var r = j.results[0];
        callback(r.latitude, r.longitude, r.name);
      }
    } catch (e) { console.log('Geocode error: ' + e); }
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
function fetchWeatherAndForecast(lat, lng, callback) {
  var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + lat +
            '&longitude=' + lng +
            '&current=temperature_2m,weather_code,wind_speed_10m' +
            '&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min' +
            '&hourly=weather_code,temperature_2m' +
            '&temperature_unit=fahrenheit' +
            '&wind_speed_unit=mph' +
            '&timezone=auto' +
            '&forecast_days=1';

  xhrRequest(url, 'GET', function (resp) {
    try {
      var j = JSON.parse(resp);
      var temp = Math.round(j.current.temperature_2m);
      var wxCode = wmoToSimple(j.current.weather_code);
      var wind = j.current.wind_speed_10m;
      if (wind > 20 && wxCode < 4) wxCode = 7;  // Windy

      var srParts = j.daily.sunrise[0].split('T')[1].split(':');
      var ssParts = j.daily.sunset[0].split('T')[1].split(':');
      var hi = Math.round(j.daily.temperature_2m_max[0]);
      var lo = Math.round(j.daily.temperature_2m_min[0]);

      // 3-hour forecast: find current hour index, grab next 3
      var now = new Date();
      var curHour = now.getHours();
      var fc = [];
      for (var h = curHour + 1; h <= curHour + 3 && h < j.hourly.time.length; h++) {
        var hTime = j.hourly.time[h];
        var hHour = parseInt(hTime.split('T')[1].split(':')[0]);
        fc.push({
          hour: hHour,
          wx: wmoToSimple(j.hourly.weather_code[h]),
          temp: Math.round(j.hourly.temperature_2m[h])
        });
      }
      // Pad to 3 if needed
      while (fc.length < 3) fc.push({ hour: 0, wx: 0, temp: 0 });

      console.log('Weather: ' + temp + 'F, hi=' + hi + ' lo=' + lo +
                  ', wx=' + wxCode + ', wind=' + wind);
      console.log('Sun: ' + srParts[0] + ':' + srParts[1] +
                  ' - ' + ssParts[0] + ':' + ssParts[1]);
      console.log('Forecast: ' + JSON.stringify(fc));

      callback({
        temperature: temp, weatherCode: wxCode,
        tempHigh: hi, tempLow: lo,
        sunriseHour: parseInt(srParts[0]), sunriseMin: parseInt(srParts[1]),
        sunsetHour: parseInt(ssParts[0]), sunsetMin: parseInt(ssParts[1]),
        forecast: fc
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
function fetchAllData() {
  console.log('Fetching data for: ' + settings.zipCode);
  geocodeZip(settings.zipCode, function (lat, lng, placeName) {
    fetchWeatherAndForecast(lat, lng, function (data) {
      if (!data) return;
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
        'DISPLAY_MODE': settings.displayMode,
        'FC_WX1': data.forecast[0].wx,
        'FC_WX2': data.forecast[1].wx,
        'FC_WX3': data.forecast[2].wx,
        'FC_H1': data.forecast[0].hour,
        'FC_H2': data.forecast[1].hour,
        'FC_H3': data.forecast[2].hour
      };
      Pebble.sendAppMessage(msg,
        function () { console.log('Data sent'); },
        function () { console.log('Send failed'); });
    });
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
      if (p.devMode !== undefined) settings.devMode = p.devMode;
    }
  } catch (e) {}
}
function saveSettings() {
  localStorage.setItem('settings', JSON.stringify(settings));
}

Pebble.addEventListener('ready', function () {
  console.log('PebbleKit JS ready');
  loadSettings();
  fetchAllData();
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload['REQUEST_DATA']) fetchAllData();
});
