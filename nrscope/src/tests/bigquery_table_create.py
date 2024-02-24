from google.cloud import bigquery
from datetime import datetime
import time
import geocoder

def create_table_with_position_and_time():
  # Get geolocation of the user
  geo_loc = geocoder.ip('me').latlng
  print(geo_loc)
  geo_str = ""
  if(float(geo_loc[0]) >= 0):
    geo_str += "P_" + str(int(geo_loc[0])) + "_" + "{:.4f}".format(int(geo_loc[0] - int(geo_loc[0])) * 1000)[2:]
  else:
    geo_str += "N_" + str(int(abs(geo_loc[0]))) + "_" + "{:.4f}".format(abs(geo_loc[0] - int(geo_loc[0])))[2:]

  if(geo_loc[1] >= 0):
    geo_str += "_P_" + str(int(geo_loc[1])) + "_" + "{:.4f}".format(geo_loc[1] - int(geo_loc[1]))[2:]
  else:
    geo_str += "_N_" + str(int(abs(geo_loc[1]))) + "_" + "{:.4f}".format(abs(geo_loc[1] - int(geo_loc[1])))[2:]
  print("Current location is: ", geo_str)

  # Get current datetime
  current_date_and_time = datetime.now()
  current_iime = current_date_and_time.strftime('%Y_%m_%d_%H_%M_%S_%f')
  print("The current time is", current_iime) 

  # Construct a BigQuery client object.
  client = bigquery.Client()


  # TODO(developer): Set table_id to the ID of the table to create.
  table_id = "tutorial-explore.ngscope_dci_log."+geo_str+"_"+current_iime

  schema = [
      bigquery.SchemaField("timestamp", "FLOAT", mode="REQUIRED"),
      bigquery.SchemaField("system_frame_index", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("slot_index", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("rnti", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("rnti_type", "STRING", mode="REQUIRED"),
      bigquery.SchemaField("k", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("mapping", "STRING", mode="REQUIRED"),
      bigquery.SchemaField("time_start", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("time_length", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("frequency_start", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("frequency_length", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("nof_dmrs_cdm_groups", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("beta_dmrs", "FLOAT", mode="REQUIRED"),
      bigquery.SchemaField("nof_layers", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("n_scid", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("tb_scaling_field", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("modulation", "STRING", mode="REQUIRED"),
      bigquery.SchemaField("mcs_index", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("transport_block_size", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("code_rate", "FLOAT", mode="REQUIRED"),
      bigquery.SchemaField("redundancy_version", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("new_data_indicator", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("nof_re", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("nof_bits", "INTEGER", mode="REQUIRED"),
      bigquery.SchemaField("mcs_table", "STRING", mode="REQUIRED"),
      bigquery.SchemaField("xoverhead", "INTEGER", mode="REQUIRED"),
  ]

  table = bigquery.Table(table_id, schema=schema)
  table = client.create_table(table)  # Make an API request.

  print(
      "Create table {}.{}.{}".format(table.project, table.dataset_id, table.table_id)
  )
  rows = []
  return client, table, schema, rows

def push_data_to_table(client, table, schema, rows_input):
  row = {'timestamp': 100.1,
       'system_frame_idex': 1021, 
       'slot_index': 102,
       'rnti': 17042,
       'rnti_type': 'c-rnti',
       'k': 0,
       'mapping': 'A',
       'time_start': 0,
       'time_length': 12,
       'frequency_start': 0,
       'frequency_length': 1,
       'nof_dmrs_cdm_groups': 1,
       'beta_dmrs': 1.14,
       'nof_layers': 2,
       'n_scid': 1,
       'tb_scaling_field': 1,
       'modulation': '16QAM',
       'mcs_index': 8,
       'transport_block_size': 512,
       'code_rate': 0.516,
       'redundancy_version': 1,
       'new_data_indicator': 1,
       'nof_re': 12,
       'nof_bits': 132,
       'mcs_table': "256qam",
       'xoverhead': 0}
  print(row)
  print(rows_input[0])
  tic = time.perf_counter()
  batch_size = len(rows_input)
  client.insert_rows(table, rows_input, schema)
    
  toc = time.perf_counter()
  print("Pushed {} entries in batch of {} to the server, taking {} s, {} s for each batch on average".format(len(rows_input), batch_size, toc-tic, (toc-tic)/(len(rows_input)/batch_size)))

if __name__ == '__main__':
  client = create_table_with_position_and_time()
  push_data_to_table(client)