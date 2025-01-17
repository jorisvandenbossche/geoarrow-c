
#' @importFrom nanoarrow infer_nanoarrow_schema
#' @export
infer_nanoarrow_schema.sfc <- function(x, ...) {
  infer_geoarrow_schema(x)
}

# Eventually we can add specializations for as_geoarrow_array() based on
# st_as_grob(), which is very fast and generates lengths + a column-major
# matrix full of buffers we can provide views into.
