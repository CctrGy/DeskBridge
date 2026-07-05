import { useEffect, useMemo, useState } from "react";
import {
  ActivityIndicator,
  Pressable,
  SafeAreaView,
  ScrollView,
  StatusBar,
  StyleSheet,
  Switch,
  Text,
  View
} from "react-native";

import { colors, radii, spacing } from "./src/theme";
import {
  fetchDeskBridgeState,
  setLightMode,
  setLightPower,
  setLightValue,
  startDeskBridgeConnection,
  subscribeToDeskBridgeState
} from "./src/services/deskbridgeClient";

const LIGHT_STEP = 4096;

export default function App() {
  const [state, setState] = useState(null);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState(false);

  async function refresh() {
    setLoading(true);
    try {
      setState(await fetchDeskBridgeState());
    } finally {
      setLoading(false);
    }
  }

  async function updateLightPower(enabled) {
    setBusy(true);
    try {
      setState(await setLightPower(enabled));
    } finally {
      setBusy(false);
    }
  }

  async function nudgeLight(field, delta) {
    if (!state?.light?.enabled) return;
    const current = Number(state.light[field] || 0);
    setBusy(true);
    try {
      setState(await setLightValue(field, current + delta));
    } finally {
      setBusy(false);
    }
  }

  async function updateLightMode(mode) {
    if (!state?.light?.enabled || state?.light?.mode === mode) return;
    setBusy(true);
    try {
      setState(await setLightMode(mode));
    } finally {
      setBusy(false);
    }
  }

  useEffect(() => {
    const unsubscribe = subscribeToDeskBridgeState((nextState) => {
      setState(nextState);
      setLoading(false);
    });
    startDeskBridgeConnection().finally(() => setLoading(false));
    return unsubscribe;
  }, []);

  const sensorCount = state?.sensors?.length || 0;
  const readySensors = useMemo(
    () => (state?.sensors || []).filter((sensor) => sensor.status === "ok").length,
    [state]
  );
  const statusLabel = state?.connected ? "Connected" : statusText(state?.connectionState);

  return (
    <SafeAreaView style={styles.safe}>
      <StatusBar barStyle="light-content" backgroundColor={colors.bgDeep} />
      <View style={styles.titlebar}>
        <View style={styles.brandRow}>
          <View style={styles.brandMark}>
            <Text style={styles.brandMarkText}>DB</Text>
          </View>
          <View>
            <Text style={styles.brandName}>DeskBridge</Text>
            <Text style={styles.brandSub}>Mobile Control</Text>
          </View>
        </View>
        <View style={styles.statusGroup}>
          <View style={[styles.statusDot, state?.connected && styles.statusDotOn]} />
          <Text style={styles.statusText}>{statusLabel}</Text>
        </View>
      </View>

      <ScrollView contentContainerStyle={styles.content}>
        <View style={styles.topActions}>
          <Text style={styles.screenTitle}>Core monitor</Text>
          <Pressable style={styles.primaryButton} onPress={refresh}>
            <Text style={styles.primaryButtonText}>{state?.connected ? "Refresh" : "Connect"}</Text>
          </Pressable>
        </View>

        {state?.lastError ? (
          <View style={styles.errorBox}>
            <Text style={styles.errorText}>{state.lastError}</Text>
          </View>
        ) : null}

        {loading && !state ? (
          <View style={styles.loadingBox}>
            <ActivityIndicator color={colors.accent} />
            <Text style={styles.muted}>Looking for DeskBridge</Text>
          </View>
        ) : (
          <>
            <View style={styles.metricsGrid}>
              <Metric label="Firmware" value={state?.firmware || "-"} />
              <Metric label="Protocol" value={state?.protocol || "-"} />
              <Metric label="Product" value={state?.productId || "-"} />
              <Metric label="Sondas" value={`${readySensors}/${sensorCount}`} />
            </View>

            <Section title="Sensor bus" rightText={state?.updatedAt || "--:--:--"}>
              {(state?.sensors || []).map((sensor) => (
                <SensorCard key={sensor.id} sensor={sensor} />
              ))}
            </Section>

            <Section title="Light control" rightText={state?.light?.enabled ? "active" : "disabled"}>
              <View style={styles.lightHeader}>
                <View>
                  <Text style={styles.cardTitle}>Strip light</Text>
                  <Text style={styles.cardSub}>{state?.light?.mode || "Unknown mode"}</Text>
                </View>
                <Switch
                  value={Boolean(state?.light?.enabled)}
                  onValueChange={updateLightPower}
                  disabled={busy}
                  trackColor={{ false: colors.line, true: colors.accent }}
                  thumbColor={colors.text}
                />
              </View>

              <View style={[styles.lightControls, !state?.light?.enabled && styles.disabledBlock]}>
                <LightStepper
                  label="Brightness"
                  value={formatPercent(state?.light?.brightness)}
                  onMinus={() => nudgeLight("brightness", -LIGHT_STEP)}
                  onPlus={() => nudgeLight("brightness", LIGHT_STEP)}
                  disabled={!state?.light?.enabled || busy}
                />
                <LightStepper
                  label="Kelvin"
                  value={String(state?.light?.kelvin ?? 0)}
                  onMinus={() => nudgeLight("kelvin", -LIGHT_STEP)}
                  onPlus={() => nudgeLight("kelvin", LIGHT_STEP)}
                  disabled={!state?.light?.enabled || busy}
                />
                <ModePicker
                  value={state?.light?.mode}
                  disabled={!state?.light?.enabled || busy}
                  onChange={updateLightMode}
                />
              </View>
            </Section>
          </>
        )}
      </ScrollView>
    </SafeAreaView>
  );
}

function statusText(value) {
  const labels = {
    demo: "Demo",
    idle: "Idle",
    scanning: "Scanning",
    connecting: "Connecting",
    waiting_bluetooth: "Bluetooth off",
    permission_denied: "No permission",
    not_found: "Not found",
    disconnected: "Disconnected",
    scan_error: "Scan error",
    connect_error: "Connect error"
  };
  return labels[value] || "Offline";
}

function formatPercent(raw) {
  const value = Math.max(0, Math.min(65535, Number(raw || 0)));
  return `${Math.round((value / 65535) * 100)}%`;
}

function Metric({ label, value }) {
  return (
    <View style={styles.metricCard}>
      <Text style={styles.label}>{label}</Text>
      <Text style={styles.metricValue} numberOfLines={1}>{value}</Text>
    </View>
  );
}

function Section({ title, rightText, children }) {
  return (
    <View style={styles.panel}>
      <View style={styles.panelTitleRow}>
        <Text style={styles.panelTitle}>{title}</Text>
        <Text style={styles.panelRight}>{rightText}</Text>
      </View>
      {children}
    </View>
  );
}

function SensorCard({ sensor }) {
  const ok = sensor.status === "ok";
  return (
    <View style={styles.sensorCard}>
      <View style={styles.sensorHead}>
        <Text style={styles.cardTitle}>{sensor.name}</Text>
        <Text style={[styles.sensorState, ok ? styles.ok : styles.warning]}>{sensor.status}</Text>
      </View>
      <View style={styles.sensorValues}>
        {sensor.values.map((item) => (
          <View key={`${sensor.id}-${item.label}`} style={styles.sensorValue}>
            <Text style={styles.label}>{item.label}</Text>
            <Text style={styles.sensorNumber}>
              {item.value}
              <Text style={styles.unit}> {item.unit}</Text>
            </Text>
          </View>
        ))}
      </View>
    </View>
  );
}

function LightStepper({ label, value, onMinus, onPlus, disabled }) {
  return (
    <View style={styles.stepper}>
      <View>
        <Text style={styles.label}>{label}</Text>
        <Text style={styles.stepperValue}>{value}</Text>
      </View>
      <View style={styles.stepperButtons}>
        <Pressable style={[styles.iconButton, disabled && styles.buttonDisabled]} onPress={onMinus} disabled={disabled}>
          <Text style={styles.iconButtonText}>-</Text>
        </Pressable>
        <Pressable style={[styles.iconButton, disabled && styles.buttonDisabled]} onPress={onPlus} disabled={disabled}>
          <Text style={styles.iconButtonText}>+</Text>
        </Pressable>
      </View>
    </View>
  );
}

function ModePicker({ value, disabled, onChange }) {
  const modes = ["one", "two", "composite"];
  return (
    <View style={styles.modePicker}>
      {modes.map((mode) => (
        <Pressable
          key={mode}
          style={[
            styles.modeButton,
            value === mode && styles.modeButtonActive,
            disabled && styles.buttonDisabled
          ]}
          onPress={() => onChange(mode)}
          disabled={disabled}
        >
          <Text style={[styles.modeButtonText, value === mode && styles.modeButtonTextActive]}>
            {mode}
          </Text>
        </Pressable>
      ))}
    </View>
  );
}

const styles = StyleSheet.create({
  safe: {
    flex: 1,
    backgroundColor: colors.bgDeep
  },
  titlebar: {
    minHeight: 82,
    paddingHorizontal: spacing.lg,
    paddingVertical: spacing.md,
    borderBottomWidth: 1,
    borderBottomColor: "rgba(255,255,255,0.07)",
    backgroundColor: "#05121f",
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between"
  },
  brandRow: {
    flexDirection: "row",
    alignItems: "center",
    gap: spacing.md
  },
  brandMark: {
    width: 46,
    height: 46,
    borderRadius: radii.md,
    backgroundColor: colors.accent,
    alignItems: "center",
    justifyContent: "center"
  },
  brandMarkText: {
    color: colors.blackInk,
    fontSize: 18,
    fontWeight: "900"
  },
  brandName: {
    color: colors.text,
    fontSize: 19,
    fontWeight: "800",
    letterSpacing: 1
  },
  brandSub: {
    color: colors.muted,
    marginTop: 2
  },
  statusGroup: {
    flexDirection: "row",
    alignItems: "center",
    gap: spacing.xs
  },
  statusDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
    backgroundColor: "#394552"
  },
  statusDotOn: {
    backgroundColor: colors.accent2
  },
  statusText: {
    color: colors.muted,
    textTransform: "uppercase",
    fontSize: 12,
    fontWeight: "700"
  },
  content: {
    padding: spacing.lg,
    gap: spacing.md
  },
  topActions: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    gap: spacing.md
  },
  screenTitle: {
    color: colors.text,
    fontSize: 24,
    fontWeight: "800"
  },
  primaryButton: {
    backgroundColor: colors.accent,
    borderRadius: radii.sm,
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.sm
  },
  primaryButtonText: {
    color: colors.blackInk,
    fontWeight: "800"
  },
  loadingBox: {
    minHeight: 180,
    borderRadius: radii.md,
    borderWidth: 1,
    borderColor: colors.line,
    backgroundColor: colors.panel,
    alignItems: "center",
    justifyContent: "center",
    gap: spacing.sm
  },
  muted: {
    color: colors.muted
  },
  errorBox: {
    borderWidth: 1,
    borderColor: "rgba(255,107,107,0.45)",
    borderRadius: radii.md,
    backgroundColor: "rgba(255,107,107,0.12)",
    padding: spacing.md
  },
  errorText: {
    color: colors.danger,
    fontSize: 13,
    fontWeight: "700"
  },
  metricsGrid: {
    flexDirection: "row",
    flexWrap: "wrap",
    gap: spacing.sm
  },
  metricCard: {
    width: "48.5%",
    minHeight: 82,
    borderWidth: 1,
    borderColor: colors.line,
    borderRadius: radii.md,
    backgroundColor: colors.panel,
    padding: spacing.md,
    justifyContent: "space-between"
  },
  label: {
    color: colors.muted,
    fontSize: 12,
    textTransform: "uppercase",
    letterSpacing: 1
  },
  metricValue: {
    color: colors.text,
    fontSize: 20,
    fontWeight: "800"
  },
  panel: {
    borderWidth: 1,
    borderColor: colors.line,
    borderRadius: radii.lg,
    backgroundColor: colors.panel,
    padding: spacing.md,
    gap: spacing.sm
  },
  panelTitleRow: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between"
  },
  panelTitle: {
    color: colors.text,
    fontSize: 17,
    fontWeight: "800"
  },
  panelRight: {
    color: colors.muted,
    fontSize: 12,
    textTransform: "uppercase"
  },
  sensorCard: {
    borderWidth: 1,
    borderColor: colors.line,
    borderRadius: radii.md,
    backgroundColor: colors.panel2,
    padding: spacing.md,
    gap: spacing.sm
  },
  sensorHead: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between"
  },
  cardTitle: {
    color: colors.text,
    fontSize: 16,
    fontWeight: "800"
  },
  cardSub: {
    color: colors.muted,
    marginTop: 3
  },
  sensorState: {
    fontSize: 12,
    fontWeight: "800",
    textTransform: "uppercase"
  },
  ok: {
    color: colors.accent2
  },
  warning: {
    color: colors.warning
  },
  sensorValues: {
    flexDirection: "row",
    flexWrap: "wrap",
    gap: spacing.sm
  },
  sensorValue: {
    minWidth: 92,
    flex: 1,
    borderRadius: radii.sm,
    backgroundColor: "#12151a",
    padding: spacing.sm
  },
  sensorNumber: {
    color: colors.text,
    fontSize: 20,
    fontWeight: "800",
    marginTop: 4
  },
  unit: {
    color: colors.muted,
    fontSize: 13,
    fontWeight: "600"
  },
  lightHeader: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    borderWidth: 1,
    borderColor: colors.line,
    borderRadius: radii.md,
    backgroundColor: colors.panel2,
    padding: spacing.md
  },
  lightControls: {
    gap: spacing.sm
  },
  disabledBlock: {
    opacity: 0.42
  },
  stepper: {
    borderWidth: 1,
    borderColor: colors.line,
    borderRadius: radii.md,
    backgroundColor: "#12151a",
    padding: spacing.md,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between"
  },
  stepperValue: {
    color: colors.text,
    fontSize: 22,
    fontWeight: "900",
    marginTop: 4
  },
  stepperButtons: {
    flexDirection: "row",
    gap: spacing.sm
  },
  iconButton: {
    width: 42,
    height: 42,
    borderRadius: radii.sm,
    borderWidth: 1,
    borderColor: colors.accent,
    backgroundColor: colors.panelSoft,
    alignItems: "center",
    justifyContent: "center"
  },
  buttonDisabled: {
    borderColor: colors.line
  },
  iconButtonText: {
    color: colors.text,
    fontSize: 24,
    fontWeight: "800"
  },
  modePicker: {
    minHeight: 48,
    flexDirection: "row",
    borderWidth: 1,
    borderColor: colors.line,
    borderRadius: radii.md,
    overflow: "hidden",
    backgroundColor: "#12151a"
  },
  modeButton: {
    flex: 1,
    alignItems: "center",
    justifyContent: "center",
    borderRightWidth: 1,
    borderRightColor: colors.line
  },
  modeButtonActive: {
    backgroundColor: colors.accent
  },
  modeButtonText: {
    color: colors.muted,
    fontSize: 12,
    fontWeight: "900",
    textTransform: "uppercase"
  },
  modeButtonTextActive: {
    color: colors.blackInk
  }
});
