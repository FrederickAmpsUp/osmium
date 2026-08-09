export interface Provider {
  id: number;
  label: string;
  unit:  string;
  dtype: string;
  dsize: number;
}

export interface Sample {
  provider: number;
  value: Uint8Array;
}
