// Shared helpers for XFA ad-hoc tests (TestXfa via -dbg-control).

import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE } from "./util.ts";

export type XfaInfo = {
  has_xfa: number;
  pure_xfa: number;
  valid: number;
  page_count: number;
  render_nonempty: number;
  render_fields: number;
  render_draws: number;
  render_borders: number;
  p1_fields: number;
  p1_draws: number;
  p1_borders: number;
  p1_lines: number;
  serialize_ok: number;
  serialize_bytes: number;
  load_error: string;
  fields_in_ps: number;
  fields_out_ps: number;
  fields_with_pa: number;
  area0: string;
  area1: string;
};

export function parseXfaLine(raw: string): XfaInfo {
  const m = raw.match(
    /has_xfa=(\d+) pure_xfa=(\d+) valid=(\d+) page_count=(\d+) render_nonempty=(\d+) render_fields=(\d+) render_draws=(\d+) render_borders=(\d+) p1_fields=(\d+) p1_draws=(\d+) p1_borders=(\d+) p1_lines=(\d+) serialize_ok=(\d+) serialize_bytes=(\d+) fields_in_ps=(\d+) fields_out_ps=(\d+) fields_with_pa=(\d+) area0=(\S*) area1=(\S*) load_error=(.*)/,
  );
  if (!m) {
    throw new Error(`unexpected TestXfa output: ${raw.trim()}`);
  }
  return {
    has_xfa: Number(m[1]),
    pure_xfa: Number(m[2]),
    valid: Number(m[3]),
    page_count: Number(m[4]),
    render_nonempty: Number(m[5]),
    render_fields: Number(m[6]),
    render_draws: Number(m[7]),
    render_borders: Number(m[8]),
    p1_fields: Number(m[9]),
    p1_draws: Number(m[10]),
    p1_borders: Number(m[11]),
    p1_lines: Number(m[12]),
    serialize_ok: Number(m[13]),
    serialize_bytes: Number(m[14]),
    load_error: m[20].trim(),
    fields_in_ps: Number(m[15]),
    fields_out_ps: Number(m[16]),
    fields_with_pa: Number(m[17]),
    area0: m[18],
    area1: m[19],
  };
}

export function formatXfaInfo(label: string, xfa: XfaInfo): string {
  const err = xfa.load_error ? ` load_error=${xfa.load_error}` : "";
  return `${label}: has_xfa=${xfa.has_xfa} pure_xfa=${xfa.pure_xfa} valid=${xfa.valid} page_count=${xfa.page_count} render_nonempty=${xfa.render_nonempty} render_fields=${xfa.render_fields} render_draws=${xfa.render_draws} render_borders=${xfa.render_borders} p1_fields=${xfa.p1_fields} p1_draws=${xfa.p1_draws} p1_borders=${xfa.p1_borders} p1_lines=${xfa.p1_lines} serialize_ok=${xfa.serialize_ok} serialize_bytes=${xfa.serialize_bytes}${err}`;
}

export async function queryXfa(pdfPath: string): Promise<XfaInfo> {
  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestXfa, [pdfPath]);
  if (exitCode !== 0) {
    throw new Error(`TestXfa failed for ${pdfPath}: ${(raw ?? "").trim()}`);
  }
  return parseXfaLine(String(raw ?? ""));
}