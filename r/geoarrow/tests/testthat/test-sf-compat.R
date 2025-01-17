
test_that("infer_nanoarrow_schema() works for mixed sfc objects", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_point(),
    sf::st_linestring()
  )
  sf::st_crs(sfc) <- "OGC:CRS84"

  schema <- infer_nanoarrow_schema(sfc)
  parsed <- geoarrow_schema_parse(schema)
  expect_identical(parsed$id, enum$Type$WKB)
  expect_identical(parsed$crs_type, enum$CrsType$PROJJSON)
})

test_that("infer_nanoarrow_schema() works for single-geometry type sfc objects", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_point(),
    sf::st_point()
  )
  sf::st_crs(sfc) <- "OGC:CRS84"

  schema <- infer_nanoarrow_schema(sfc)
  parsed <- geoarrow_schema_parse(schema)
  expect_identical(parsed$geometry_type, enum$GeometryType$POINT)
  expect_identical(parsed$dimensions, enum$Dimensions$XY)
  expect_identical(parsed$crs_type, enum$CrsType$PROJJSON)
})

test_that("infer_nanoarrow_schema() works for single-geometry type sfc objects (Z)", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_point(c(1, 2, 3)),
    sf::st_point(c(1, 2, 3))
  )
  sf::st_crs(sfc) <- "OGC:CRS84"

  schema <- infer_nanoarrow_schema(sfc)
  parsed <- geoarrow_schema_parse(schema)
  expect_identical(parsed$geometry_type, enum$GeometryType$POINT)
  expect_identical(parsed$dimensions, enum$Dimensions$XYZ)
  expect_identical(parsed$crs_type, enum$CrsType$PROJJSON)
})

test_that("infer_nanoarrow_schema() works for single-geometry type sfc objects (M)", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_point(c(1, 2, 3), dim = "XYM"),
    sf::st_point(c(1, 2, 3), dim = "XYM")
  )
  sf::st_crs(sfc) <- "OGC:CRS84"

  schema <- infer_nanoarrow_schema(sfc)
  parsed <- geoarrow_schema_parse(schema)
  expect_identical(parsed$geometry_type, enum$GeometryType$POINT)
  expect_identical(parsed$dimensions, enum$Dimensions$XYM)
  expect_identical(parsed$crs_type, enum$CrsType$PROJJSON)
})

test_that("infer_nanoarrow_schema() works for single-geometry type sfc objects (ZM)", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_point(c(1, 2, 3, 4)),
    sf::st_point(c(1, 2, 3, 4))
  )
  sf::st_crs(sfc) <- "OGC:CRS84"

  schema <- infer_nanoarrow_schema(sfc)
  parsed <- geoarrow_schema_parse(schema)
  expect_identical(parsed$geometry_type, enum$GeometryType$POINT)
  expect_identical(parsed$dimensions, enum$Dimensions$XYZM)
  expect_identical(parsed$crs_type, enum$CrsType$PROJJSON)
})

test_that("as_nanoarrow_array() works for mixed sfc", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_point(c(NaN, NaN)),
    sf::st_linestring()
  )

  array <- as_geoarrow_array(sfc)
  expect_identical(
    as.raw(array$buffers[[3]]),
    unlist(sf::st_as_binary(sfc))
  )
})

test_that("as_nanoarrow_array() works for sfc_POINT", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_point(c(1, 2)),
    sf::st_point(c(3, 4))
  )

  array <- as_geoarrow_array(sfc)
  expect_identical(
    as.raw(array$children[[1]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(1, 3)))
  )
  expect_identical(
    as.raw(array$children[[2]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(2, 4)))
  )
})

test_that("as_nanoarrow_array() works for sfc_LINESTRING", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_linestring(rbind(c(1, 2), c(3, 4)))
  )

  array <- as_geoarrow_array(sfc)
  expect_identical(
    as.raw(array$children[[1]]$children[[1]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(1, 3)))
  )
  expect_identical(
    as.raw(array$children[[1]]$children[[2]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(2, 4)))
  )
})

test_that("as_nanoarrow_array() works for sfc_POLYGON", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_polygon(list(rbind(c(1, 2), c(3, 4), c(5, 6), c(1, 2))))
  )

  array <- as_geoarrow_array(sfc)
  expect_identical(
    as.raw(array$children[[1]]$children[[1]]$children[[1]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(1, 3, 5, 1)))
  )
  expect_identical(
    as.raw(array$children[[1]]$children[[1]]$children[[2]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(2, 4, 6, 2)))
  )
})

test_that("as_nanoarrow_array() works for sfc_MULTIPOINT", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_multipoint(rbind(c(1, 2), c(3, 4)))
  )

  array <- as_geoarrow_array(sfc)
  expect_identical(
    as.raw(array$children[[1]]$children[[1]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(1, 3)))
  )
  expect_identical(
    as.raw(array$children[[1]]$children[[2]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(2, 4)))
  )
})

test_that("as_nanoarrow_array() works for sfc_MULTILINESTRING", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_multilinestring(list(rbind(c(1, 2), c(3, 4), c(5, 6), c(1, 2))))
  )

  array <- as_geoarrow_array(sfc)
  expect_identical(
    as.raw(array$children[[1]]$children[[1]]$children[[1]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(1, 3, 5, 1)))
  )
  expect_identical(
    as.raw(array$children[[1]]$children[[1]]$children[[2]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(2, 4, 6, 2)))
  )
})

test_that("as_nanoarrow_array() works for sfc_MULTIPOLYGON", {
  skip_if_not_installed("sf")

  sfc <- sf::st_sfc(
    sf::st_multipolygon(list(list(rbind(c(1, 2), c(3, 4), c(5, 6), c(1, 2)))))
  )

  array <- as_geoarrow_array(sfc)
  expect_identical(
    as.raw(array$children[[1]]$children[[1]]$children[[1]]$children[[1]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(1, 3, 5, 1)))
  )
  expect_identical(
    as.raw(array$children[[1]]$children[[1]]$children[[1]]$children[[2]]$buffers[[2]]),
    as.raw(nanoarrow::as_nanoarrow_buffer(c(2, 4, 6, 2)))
  )
})
