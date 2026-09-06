import { API_HEADERS } from './constants';
import type {
  AudioStatusResponse,
  BootstrapResponse,
  DisplayStatusResponse,
  EffectsStatusResponse,
  GeminiModuleResponse,
  OpenAiModuleResponse,
  PortalResponse,
  PowerRuntimeStatus,
  SleepStatusResponse,
  TalkingClockModuleResponse,
  TimeRuntimeStatus,
  TimeSettingsResponse,
  TimezoneListResponse,
  UpdateRuntimeResponse,
  XiaozhiModuleResponse,
} from './types';

// Every caller here sets a busy flag before awaiting and only clears it in a `finally` --
// without a timeout, a request the device never answers (e.g. mid Wi-Fi-scan) leaves that
// flag permanently true, wedging the corresponding button until the page is reloaded.
const REQUEST_TIMEOUT_MS = 10000;

export async function fetchApiJson<T>(path: string, init?: RequestInit): Promise<T> {
  const timeoutController = new AbortController();
  const timeoutId = setTimeout(() => timeoutController.abort(), REQUEST_TIMEOUT_MS);

  let response: Response;
  try {
    response = await fetch(path, {
      cache: 'no-store',
      ...init,
      signal: init?.signal ?? timeoutController.signal,
      headers: {
        ...API_HEADERS,
        ...(init?.headers || {}),
      },
    });
  } catch (error) {
    if (timeoutController.signal.aborted) {
      throw new Error(`Request to ${path} timed out. Check the device is still reachable.`);
    }
    throw error;
  } finally {
    clearTimeout(timeoutId);
  }

  const contentType = response.headers.get('content-type') || '';
  const bodyText = await response.text();

  if (!contentType.includes('application/json')) {
    throw new Error(
      `Unexpected response for ${path}. ` +
        'Make sure you are connected to the device AP and loading the portal from the ESP32.'
    );
  }

  const data = JSON.parse(bodyText) as { success?: boolean; message?: string };
  if (!response.ok || data.success !== true) {
    throw new Error(data.message || 'Request failed.');
  }

  return data as T;
}

export const fetchPortalJson = (path: string, init?: RequestInit) =>
  fetchApiJson<PortalResponse>(path, init);
export const fetchTimeSettingsJson = (path: string, init?: RequestInit) =>
  fetchApiJson<TimeSettingsResponse>(path, init);
export const fetchTimeRuntimeJson = (path: string, init?: RequestInit) =>
  fetchApiJson<{ success: boolean; message?: string; runtime?: TimeRuntimeStatus }>(path, init);
export const fetchPowerRuntimeJson = (path: string, init?: RequestInit) =>
  fetchApiJson<{ success: boolean; message?: string; runtime?: PowerRuntimeStatus }>(path, init);
export const fetchUpdateRuntimeJson = (path: string, init?: RequestInit) =>
  fetchApiJson<UpdateRuntimeResponse>(path, init);
export const fetchAudioStatusJson = (path: string, init?: RequestInit) =>
  fetchApiJson<AudioStatusResponse>(path, init);
export const fetchDisplayStatusJson = (path: string, init?: RequestInit) =>
  fetchApiJson<DisplayStatusResponse>(path, init);
export const fetchSleepStatusJson = (path: string, init?: RequestInit) =>
  fetchApiJson<SleepStatusResponse>(path, init);
export const fetchBootstrapJson = (path: string, init?: RequestInit) =>
  fetchApiJson<BootstrapResponse>(path, init);
export const fetchEffectsJson = (path: string, init?: RequestInit) =>
  fetchApiJson<EffectsStatusResponse>(path, init);
export const fetchTalkingClockModuleJson = (path: string, init?: RequestInit) =>
  fetchApiJson<TalkingClockModuleResponse>(path, init);
export const fetchGeminiModuleJson = (path: string, init?: RequestInit) =>
  fetchApiJson<GeminiModuleResponse>(path, init);
export const fetchOpenAiModuleJson = (path: string, init?: RequestInit) =>
  fetchApiJson<OpenAiModuleResponse>(path, init);
export const fetchXiaozhiModuleJson = (path: string, init?: RequestInit) =>
  fetchApiJson<XiaozhiModuleResponse>(path, init);
export const fetchTimezoneListJson = (path: string, init?: RequestInit) =>
  fetchApiJson<TimezoneListResponse>(path, init);
