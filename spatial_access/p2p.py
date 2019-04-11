# Logan Noel (github.com/lmnoel)
#
# ©2017-2019, Center for Spatial Data Science


import time
import logging
import os
import scipy.spatial
from geopy import distance
import pandas as pd

from spatial_access.MatrixInterface import MatrixInterface
from spatial_access.NetworkInterface import NetworkInterface
from spatial_access.ConfigInterface import ConfigInterface

from spatial_access.SpatialAccessExceptions import PrimaryDataNotFoundException
from spatial_access.SpatialAccessExceptions import SecondaryDataNotFoundException
from spatial_access.SpatialAccessExceptions import UnableToParsePrimaryDataException
from spatial_access.SpatialAccessExceptions import UnableToParseSecondaryDataException
from spatial_access.SpatialAccessExceptions import UnknownModeException
from spatial_access.SpatialAccessExceptions import InsufficientDataException
from spatial_access.SpatialAccessExceptions import DuplicateInputException
from spatial_access.SpatialAccessExceptions import WriteTMXFailedException
from spatial_access.SpatialAccessExceptions import WriteCSVFailedException

# TODO: improve logging granularity
# TODO: disable logs writing to disk

class TransitMatrix:
    """
    Compute transit matrices at scale.
    """
    def __init__(
            self,
            network_type,
            primary_input=None,
            secondary_input=None,
            read_from_tmx=None,
            primary_hints=None,
            secondary_hints=None,
            disable_area_threshold=False,
            walk_speed=None,
            bike_speed=None,
            epsilon=0.05,
            debug=False):
        """
        Args:
            network_type: string, one of {'walk', 'bike', 'drive', 'meters'}.
            primary_input: string, csv filename.
            secondary_input: string, csv filename.
            read_from_tmx: string, tmx filename.
            primary_hints: dictionary, map column names to expected values.
            secondary_hints: dictionary, map column names to expected values.
            disable_area_threshold: boolean, enable if computation fails due to
            exceeding bounding box area constraint.
            walk_speed: numeric, override default walking speed (km/hr).
            bike_speed: numeric, override default walking speed (km/hr).
            epsilon: numeric, factor by which to increase the requested bounding box.
                Increasing epsilon may result in increased accuracy for points
                at the edge of the bounding box, but will increase computation times.
            debug: boolean, enable to see more detailed logging output.

        Raises:
            UnknownModeException: If the network type is unknown.
            DuplicateInputException: If the same file is given as primary_input
                and secondary_input. To compute symmetric matrices (NxN), leave the
                secondary input field blank.
            InsufficientDataException: If neither a source data file (csv) nor a
                transit matrix file (tmx) is supplied.

        """

        # arguments
        self.network_type = network_type
        self.epsilon = epsilon
        self.primary_input = primary_input
        self.secondary_input = secondary_input
        self.primary_hints = primary_hints
        self.secondary_hints = secondary_hints

        # member variables
        self.primary_data = None
        self.secondary_data = None

        # start the logger
        self.logger = None
        self.set_logging(debug)

        # instantiate interfaces
        self._config_interface = ConfigInterface(network_type, logger=self.logger)
        self._network_interface = NetworkInterface(network_type, logger=self.logger,
                                                   disable_area_threshold=disable_area_threshold)

        self.matrix_interface = MatrixInterface(primary_input_name=primary_input,
                                                secondary_input_name=secondary_input,
                                                logger=self.logger)

        if walk_speed is not None:
            self._config_interface.update_walking_speed(walk_speed=walk_speed)
        if bike_speed is not None:
            self._config_interface.update_biking_speed(bike_speed=bike_speed)

        if network_type not in {'drive', 'walk', 'bike', 'meters'}:
            raise UnknownModeException()

        self._use_meters = network_type == 'meters'

        if self.primary_input == self.secondary_input and self.primary_input is not None:
            raise DuplicateInputException("Gave duplicate inputs: {}".format(self.primary_input))

        # need to supply either:
        if primary_input is None and read_from_tmx is None:
            raise InsufficientDataException()

        if read_from_tmx:
            self.matrix_interface.read_tmx(read_from_tmx)

    def set_logging(self, debug):
        """
        Set the logging level.

        Args:
            debug: enable for increased details
                in logs.
        """
        if debug:
            logging.basicConfig(level=logging.DEBUG)
        else:
            logging.basicConfig(level=logging.INFO)
        self.logger = logging.getLogger(__name__)
        self.logger.debug("Running in debug mode")

    @staticmethod
    def _get_output_filename(keyword, extension):
        """
        Args:
            keyword: the file's keyword.
            extension: the files's type.

        Returns: unique filename.
        """
        if not os.path.exists("data/matrices/"):
            os.makedirs("data/matrices/")
        if extension is None:
            filename = 'data/matrices/{}_0'.format(keyword)
        else:
            filename = 'data/matrices/{}_0.{}'.format(keyword, extension)
        counter = 1
        while os.path.isfile(filename):
            if extension is None:
                filename = 'data/matrices/{}_{}'.format(
                            keyword, counter)
            else:    
                filename = 'data/matrices/{}_{}.{}'.format(
                    keyword, counter, extension)
            counter += 1

        return filename

    def _parse_csv(self, primary):
        """
        Load source data from .csv. Identify lon, lon and id columns.

        Args:
            primary: boolean, true if loading primary data.
        Raises:
            UnableToParsePrimaryDataException: The user's supplied
                mapping to column names failed.
            UnableToParseSecondaryDataException: The user's supplied
                mapping to column names failed.
        """
        if primary:
            filename = self.primary_input
        else:
            filename = self.secondary_input

        source_data = pd.read_csv(filename)
        source_data_columns = source_data.columns.values

        # extract the column names
        lon = ''
        lat = ''
        idx = ''
        skip_user_input = False
        # use the column names if we already have them

        try:
            if primary and self.primary_hints:
                lon = self.primary_hints['lon']
                lat = self.primary_hints['lat']
                idx = self.primary_hints['idx']
                skip_user_input = True
            elif not primary and self.secondary_hints:
                lon = self.secondary_hints['lon']
                lat = self.secondary_hints['lat']
                idx = self.secondary_hints['idx']
                skip_user_input = True

        except KeyError:
            # raise immediately to let the user know there is a problem
            if primary:
                self.logger.error('Unable to use primary_hints to read sources')
                raise UnableToParsePrimaryDataException('Unable to use primary_hints to read sources')
            else:
                self.logger.error('Unable to use secondary_hints to read dests')
                raise UnableToParseSecondaryDataException('Unable to use secondary_hints to read sources')
    
        if not skip_user_input:
            print('The variables in your data set are:')
            for var in source_data_columns:
                print('> ', var)
            while lon not in source_data_columns:
                lon = input('Enter the longitude coordinate: ')
            while lat not in source_data_columns:
                lat = input('Enter the latitude coordinate: ')
            while idx not in source_data_columns:
                idx = input('Enter the index name: ')

        # drop nan lines
        pre_drop = len(source_data)
        source_data.dropna(subset=[lon, lat], axis='index', inplace=True)

        dropped_lines = pre_drop - len(source_data)

        keyword = "rows" if primary else "columns"
        self.logger.debug(
            'Total number of {} in the dataset: {}'.format(keyword, pre_drop))
        if dropped_lines > 0:
            self.logger.warning(
                "Rows dropped due to missing latitude or longitude values: %d", dropped_lines)

        # set index and clean
        if primary:
            self.matrix_interface.primary_ids_are_string = source_data[idx].dtype != int
        else:
            self.matrix_interface.secondary_ids_are_string = source_data[idx].dtype != int
        source_data.set_index(idx, inplace=True)

        source_data.rename(columns={lon: 'lon', lat: 'lat'}, inplace=True)
        if primary:
            self.primary_data = source_data[['lon', 'lat']]
            self.primary_hints = {'idx': idx, 'lon': lon, 'lat': lat}
        else:
            self.secondary_data = source_data[['lon', 'lat']]
            self.secondary_hints = {'idx': idx, 'lon': lon, 'lat': lat}

    def _load_inputs(self):
        """
        Load one input file if the user wants a symmetric
        transit matrix, or two for an asymmetric matrix.

        Raises:
            PrimaryDataNotFoundException: Primary data isn't found.
            SecondaryDataNotFoundException: Secondary data isn't found.
        """
        if not os.path.isfile(self.primary_input):
            self.logger.error("Unable to find primary csv.")
            raise PrimaryDataNotFoundException("Unable to find primary csv")
        if self.secondary_input:
            if not os.path.isfile(self.secondary_input):
                self.logger.error("Unable to find secondary csv.")
                raise SecondaryDataNotFoundException("Unable to find secondary csv")
        else:
            self.matrix_interface.secondary_ids_are_string = self.matrix_interface.primary_ids_are_string

        self._parse_csv(True)
        if self.secondary_input:
            self._parse_csv(False)

    def _cost_model(self, distance, speed_limit):
        """
        This method will be removed.
        """
        if self.network_type == 'walk':
            return int((distance / self._config_interface.WALK_CONSTANT) +
                       self._config_interface.WALK_NODE_PENALTY)
        elif self.network_type == 'bike':
            return int((distance / self._config_interface.BIKE_CONSTANT) +
                       self._config_interface.BIKE_NODE_PENALTY)

        drive_constant = (speed_limit / self._config_interface.ONE_HOUR)
        drive_constant *= self._config_interface.ONE_KM
        return int((distance / drive_constant) + self._config_interface.DRIVE_NODE_PENALTY)

    def _reduce_node_indeces(self):
        """
        Map the network indeces to location.
        Returns:
            dictionary of {node index : node location}
        """
        simple_node_indeces = {}
        for position, id_ in enumerate(self._network_interface.nodes['id']):
            simple_node_indeces[id_] = position
        return simple_node_indeces

    # TODO: do this using data frame methods instead
    def _parse_network(self):
        """
        Cleans and generates the city network.
        """

        start_time = time.time()

        FROM_IDX = self._network_interface.edges.columns.get_loc('from') + 1
        TO_IDX = self._network_interface.edges.columns.get_loc('to') + 1
        
        # map index name to position
        simple_node_indeces = self._reduce_node_indeces()

        # create a mapping of each node to every other connected node
        # transform them by cost model as well
        for data in self._network_interface.edges.itertuples():
            from_idx = data[FROM_IDX]
            to_idx = data[TO_IDX]
            if self._use_meters:
                impedance = data.distance
            else:
                highway_tag = data.highway
                if highway_tag is None or highway_tag not in self._config_interface.speed_limit_dict["urban"]:
                    highway_tag = "unclassified"
                impedance = self._cost_model(data.distance,
                                             self._config_interface.speed_limit_dict["urban"][highway_tag])

            is_bidirectional = data.oneway != 'yes' or self.network_type != 'drive'
            self.matrix_interface.add_edge_to_graph(simple_node_indeces[from_idx],
                                                    simple_node_indeces[to_idx],
                                                    impedance, is_bidirectional)
        time_delta = time.time() - start_time
        self.logger.debug("Prepared raw network in {:,.2f} seconds".format(time_delta))

    def _match_to_nearest_neighbor(self, is_primary=True, is_also_secondary=False):
        """
        Map each vertex in the user's data set to a vertex in
        the underlying osm network.

        Args:
            is_primary: true if this is the primary dataset.
            is_also_secondary: true if this is also acting as the secondary dataset.
        """

        if is_primary:
            data = self.primary_data
        else:
            data = self.secondary_data

        nodes = self._network_interface.nodes[['x', 'y']]

        start_time = time.time()

        # make a kd tree in the lat, long dimension
        node_array = nodes.values
        kd_tree = scipy.spatial.cKDTree(node_array)

        # map each node in the source/dest data to the nearest
        # corresponding node in the OSM network
        # and write to file
        for row in data.itertuples():
            origin_id, origin_x, origin_y = row
            latlong_diff, node_loc = kd_tree.query([origin_x, origin_y], k=1)
            node_number = nodes.index[node_loc]
            origin_location = (origin_y, origin_x)
            closest_node_location = (nodes.loc[node_number].y,
                                                       nodes.loc[node_number].x)

            # keep track of nodes that are used to snap a user data point
            self._network_interface.user_node_friends.add(node_number)
            edge_distance = distance.distance(origin_location, closest_node_location).m

            edge_weight = int(edge_distance / self._config_interface.default_edge_cost)

            if is_primary:
                self.matrix_interface.add_user_source_data(network_id=node_loc,
                                                           user_id=origin_id,
                                                           weight=edge_weight,
                                                           is_also_dest=is_also_secondary)
            else:
                self.matrix_interface.add_user_dest_data(network_id=node_loc,
                                                         user_id=origin_id,
                                                         weight=edge_weight)

        time_delta = time.time() - start_time
        self.logger.debug(
            'Nearest Neighbor matching completed in {:,.2f} seconds'.format(time_delta))

    def write_csv(self, outfile=None):
        """
        Write the transit matrix to csv.

        Note: Use write_tmx (as opposed to this method) to
        save the transit matrix unless exporting for external use.

        Arguments:
            outfile: optional filename.
        Raises:
            WriteCSVFailedException: filename does not have correct extension.
        """
        if not outfile:
            outfile = self._get_output_filename(self.network_type, extension='csv')
        if '.csv' not in outfile:
            raise WriteCSVFailedException('given filename does not have the correct extension (.csv)')
        self.matrix_interface.write_csv(outfile)

    def write_tmx(self, outfile=None):
        """
        Write the transit matrix to tmx.

        Note: Use this method (as opposed to write_csv) to
        save the transit matrix unless exporting data for
        external use.

        Arguments:
            outfile: optional filename.
        Raises:
            WriteTMXFailedException: filename does not have correct extension.
        """
        if not outfile:
            outfile = self._get_output_filename(self.network_type, extension='tmx')
        if '.tmx' not in outfile:
            raise WriteTMXFailedException('given filename does not have the correct extension (.tmx)')
        self.matrix_interface.write_tmx(outfile)

    def prefetch_network(self):
        """
        Fetch and cache the osm network.
        """
        self._load_inputs()

        self._network_interface.load_network(self.primary_data,
                                             self.secondary_data,
                                             self.secondary_input is not None,
                                             self.epsilon)
    @staticmethod
    def clear_cache():
        """
        Clear the network cache.
        """
        NetworkInterface.clear_cache()

    def _is_compressible(self):
        """
        Returns: true if the transit matrix can be compressed by
            half without losing any data.
        """
        return self._is_symmetric() and self.network_type in {'walk', 'bike'}

    def _is_symmetric(self):
        """
        Returns: true if the transit matrix is NxN, that is, has
            the same origins and destinations.
        """
        return self.secondary_input is None

    def process(self):
        """
        - Load the users's data.
        - Fetch the osm network.
        - Parse the network.
        - Calculate transit matrix.
        """
        start_time = time.time()

        self.logger.debug("Processing network (%s) with epsilon: %f",
            self.network_type, self.epsilon)

        self.prefetch_network()

        rows = len(self.primary_data)

        if self.secondary_input is None:
            cols = rows
            self.matrix_interface.secondary_ids_are_string = self.matrix_interface.primary_ids_are_string
        else:
            cols = len(self.secondary_data)
        self.matrix_interface.prepare_matrix(is_symmetric=self._is_symmetric(),
                                             is_compressible=self._is_compressible(),
                                             rows=rows,
                                             columns=cols,
                                             network_vertices=self._network_interface.number_of_nodes())

        if self.secondary_input:
            self._match_to_nearest_neighbor(is_primary=True, is_also_secondary=False)
            self._match_to_nearest_neighbor(is_primary=False, is_also_secondary=False)
        else:
            self._match_to_nearest_neighbor(is_primary=True, is_also_secondary=True)

        self._parse_network()

        # offload primary and secondary input data frames because we don't need them anymore
        self.primary_input = None
        self.secondary_input = None

        self.matrix_interface.build_matrix()
        time_delta = time.time() - start_time

        self.logger.info('All operations completed in {:,.2f} seconds'.format(time_delta))
