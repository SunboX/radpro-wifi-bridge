# Prometheus metrics

The bridge exposes its current detector readings at `http://<device-ip>/metrics`
on the same HTTP server as the web portal. Prometheus can scrape this endpoint
without MQTT or a separate exporter, and Grafana can display the stored readings
through a Prometheus data source.

Add a scrape job to `prometheus.yml`, replacing the example IP with your bridge:

```yaml
scrape_configs:
  - job_name: radpro
    scrape_interval: 15s
    static_configs:
      - targets: ["192.168.1.50:80"]
```

The default Prometheus metrics path is `/metrics`.

| Metric | Type | Unit / meaning |
| --- | --- | --- |
| `radpro_info` | Gauge | Constant `1`, with manufacturer, model, detector firmware, bridge firmware, device ID, locale and device power labels |
| `radpro_tube_rate` | Gauge | Counts per minute |
| `radpro_tube_dose_rate` | Gauge | Microsieverts per hour |
| `radpro_tube_pulse_count` | Counter | Detector pulse count; can reset when the detector resets |
| `radpro_battery_voltage` | Gauge | Volts |
| `radpro_battery_percent` | Gauge | Battery percentage estimated by the bridge from battery voltage |

The endpoint exports the latest readings already collected by the bridge.
Scraping it does not trigger a detector read. Unavailable readings are omitted,
including during startup, after USB disconnect, or while measurements are
cleared because the detector is off or stale. A missing sensitivity may leave
the dose-rate metric unavailable while CPM remains available. Actual zero
readings are exported as zero.

Device labels support UTF-8. Backslashes, double quotes and line feeds are escaped
according to the Prometheus text format.
