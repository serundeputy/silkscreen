<?php

/**
 * @file
 * This is the API for configuration storage.
 */

/**
 * Retrieves a configuration object.
 *
 * This is the main entry point to the configuration API. Calling
 * @code config(book.admin) @endcode will return a configuration object in which
 * the book module can store its administrative settings.
 *
 * @param string $config_file
 *   The name of the configuration object to retrieve. The name corresponds to
 *   an JSON configuration file. For @code config(book.admin) @endcode, the
 *   config object returned will contain the contents of book.admin.json.
 * @param string $type
 *   (optional) The type of config directory to return. Backdrop core provides
 *   'active' and 'staging'. Defaults to 'active'.
 *
 * @return Config
 *   A Config object containing the specified configuration settings.
 *
 */
function config($config_file, $type = 'active') {
  // Use the advanced backdrop_static() pattern, since this is called very often.
  static $backdrop_static_fast;
  if (!isset($backdrop_static_fast)) {
    $backdrop_static_fast['loaded_configs'] = &backdrop_static(__FUNCTION__);
  }
  $loaded_configs = &$backdrop_static_fast['loaded_configs'];

  if (!isset($loaded_configs[$type][$config_file])) {
    $storage = config_get_config_storage($type);
    $config = new Config($config_file, $storage);
    $config->load();
    $cache = $config->get('_config_static');
    if ($cache) {
      $loaded_configs[$type][$config_file] = $config;
    }
  }
  else {
    $config = $loaded_configs[$type][$config_file];
  }

  return $config;
}

/**
 * A shortcut function to load and retrieve a single value from a config file.
 *
 * @param string $config_file
 *   The name of the configuration object to retrieve. The name corresponds to
 *   an JSON configuration file. For @code config(book.admin) @endcode, the
 *   config object returned will contain the contents of book.admin.json.
 * @param string $option
 *   The name of the config option within the file to read. The config option
 *   may contain periods to indicate levels within the config file. If NULL is
 *   passed in, the entire contents of the config file will be returned.
 *
 * @return mixed
 *   The contents of the requested config option. Returns NULL if the specified
 *   option was not found in the file at all.
 */
function config_get($config_file, $option = NULL) {
  $config = config($config_file);
  return $config->get($option);
}

/**
 * A shortcut function to set and save a single value in a config file.
 *
 * Note that this function immediately writes the config file to disk and clears
 * associated caches related to the new config. If writing a number of options
 * to the same configuration file, it is better to create a config object
 * directly, set all the new values, and then save the config file with all the
 * new options all at once.
 *
 * @param string $config_file
 *   The name of the configuration object to retrieve. The name corresponds to
 *   an JSON configuration file. For @code config(book.admin) @endcode, the
 *   config object returned will contain the contents of book.admin.json.
 * @param string $option
 *   The name of the config option within the file to set. The config option
 *   may contain periods to indicate levels within the config file.
 * @param mixed $value
 *   The value to save into the config file.
 */
function config_set($config_file, $option, $value) {
  $config = config($config_file);
  $config->set($option, $value);
  $config->save();
}

/**
 * A shortcut function to set and save multiple values in a config file.
 *
 * Note that this function immediately writes the config file to disk and clears
 * associated caches related to the new config.  Unlike config_set() which would
 * need to be called once per value, this function updates all the values and
 * then saves the object.
 *
 * @param string $config_file
 *   The name of the configuration object to retrieve. The name corresponds to
 *   an JSON configuration file. For @code config(book.admin) @endcode, the
 *   config object returned will contain the contents of book.admin.json.
 * @param array $options
 *   A keyed array of configuration option names mapping their values.
 */
function config_set_multiple($config_file, $options) {
  $config = config($config_file);
  $config->setMultiple($options);
  $config->save();
}

/**
 * Returns the path of a configuration directory.
 *
 * @param string $type
 *   (optional) The type of config directory to return. Backdrop core provides
 *   'active' and 'staging'. Defaults to 'active'.
 *
 * @return string
 *   The configuration directory path.
 *
 * @throws ConfigException
 */
function config_get_config_directory($type = 'active') {
  global $config_directories;

  if ($test_prefix = backdrop_valid_test_ua()) {
    // See BackdropWebTestBase::setUp().
    $path = conf_path() . '/files/simpletest/' . substr($test_prefix, 10) . '/config_' . $type;
  }
  elseif (!empty($config_directories[$type])) {
    $path = $config_directories[$type];

    // If the path contains a colon, assume it is a storage URL like
    // db:/default/config_active and return it directly.
    if (strpos($path, ':') !== FALSE) {
      return $path;
    }

    // If the path starts with a slash or dot, assume a normal path. If just
    // a directory name is provided, make it relative to the settings.php file.
    $first_character = substr($path, 0, 1);
    if (!in_array($first_character, array('.', '/', '\\'))) {
      $path = conf_path() . '/' . $path;
    }
  }
  else {
    throw new ConfigException(format_string('The configuration directory type "@type" does not exist.', array('@type' => $type)));
  }
  return $path;
}

/**
 * Retrieves all configurations starting with a particular prefix.
 *
 * @param string $prefix
 *   The prefix of the configuration names to retrieve.
 * @param string $type
 *   The configuration type, either "staging" or "active".
 *
 * @return array
 *   An array containing matching configuration object names.
 */
function config_get_names_with_prefix($prefix, $type = 'active') {
  $storage = config_get_config_storage($type);
  return $storage->listAll($prefix);
}

/**
 * Loads configuration objects by name.
 *
 * @param array $names
 *   The list of configuration names to load.
 * @param string $type
 *   The configuration type, either "staging" or "active".
 *
 * @return array
 *   An array containing matching configuration objects.
 */
function config_load_multiple($names, $type = 'active') {
  $storage = config_get_config_storage($type);
  return $storage->readMultiple($names);
}

/**
 * Moves the default config supplied by a module to the live config directory.
 *
 * @param string $module
 *   The name of the module we are installing.
 * @param string|NULL $config_name
 *   (optional) If wanting to copy just a single configuration file from the
 *   module, specify the configuration file name without the extension.
 */
function config_install_default_config($module, $config_name = NULL) {
  $module_config_dir = backdrop_get_path('module', $module) . '/config';
  $storage = new ConfigFileStorage($module_config_dir);
  if (is_dir(backdrop_get_path('module', $module) . '/config')) {
    $files = glob($module_config_dir . '/' . '*.json');
    foreach ($files as $file) {
      // Load config data into the active store and write it out to the
      // file system in the Backdrop config directory. Note the config name
      // needs to be the same as the file name WITHOUT the extension.
      $parts = explode('/', $file);
      $file = array_pop($parts);
      $file_config_name = str_replace('.json', '', $file);
      if (is_null($config_name) || $file_config_name === $config_name) {
        $data = $storage->read($file_config_name);
        $config = config($file_config_name);
        // We only create new configs, and do not overwrite existing ones.
        if ($config->isNew()) {
          $config->setData($data);
          $config->save();
        }
      }
    }
  }
}

/**
 * Uninstall all the configuration provided by a module.
 */
function config_uninstall_config($module, $config_name = NULL) {
  backdrop_load('module', $module);
  if ($configs = module_invoke($module, 'config_info')) {
    foreach ($configs as $config_name => $config_info) {
      if (isset($config_info['name_key'])) {
        $sub_names = config_get_names_with_prefix($config_name . '.');
        foreach ($sub_names as $sub_name) {
          config($sub_name)->delete();
        }
      }
      else {
        config($config_name)->delete();
      }
    }
  }
}

/**
 * Get the storage object for the specified configuration type
 *
 * @param string $type
 *   (optional) The type of config directory to return. Backdrop core provides
 *   'active' and 'staging'. Defaults to 'active'.
 *
 * @return ConfigStorageInterface
 *   A ConfigStorageInterface object managing the specified configuration type.
 */
function config_get_config_storage($type = 'active') {
  $directory = config_get_config_directory($type);
  $colon_pos = strpos($directory, ':');
  if ($colon_pos !== FALSE) {
    $storage_type = substr($directory, 0, $colon_pos);
    switch ($storage_type) {
      case 'db':
        return new ConfigDatabaseStorage($directory);
      case 'file':
        return new ConfigFileStorage(substr($directory, $colon_pos+1));
      default:
        // fall through
    }
  }
  return new ConfigFileStorage($directory);
}

/**
 * A base exception thrown in any configuration system operations.
 */
class ConfigException extends Exception {}

/**
 * Exception thrown when a config object name is invalid.
 */
class ConfigNameException extends ConfigException {}

/**
 * Exception thrown when a config object has a validation error before saving.
 *
 * Messages thrown using ConfigValidateException should be translated, as they
 * are passed directly to end-users during form validations.
 */
class ConfigValidateException extends ConfigException {}

/**
 * Exception thrown by classes implementing ConfigStorageInterface.
 */
class ConfigStorageException extends ConfigException {}

/**
 * Exception thrown when attempting to read a config file fails.
 */
class ConfigStorageReadException extends ConfigStorageException {}

/**
 * Defines the default configuration object.
 */
class Config {

  /**
   * The maximum length of a configuration object name.
   *
   * Many filesystems (including HFS, NTFS, and ext4) have a maximum file name
   * length of 255 characters. To ensure that no configuration objects
   * incompatible with this limitation are created, we enforce a maximum name
   * length of 250 characters (leaving 5 characters for the file extension).
   *
   * @see http://en.wikipedia.org/wiki/Comparison_of_file_systems
   */
  const MAX_NAME_LENGTH = 250;

  /**
   * The name of the configuration object.
   *
   * @var string
   */
  protected $name;

  /**
   * Whether the configuration object is new or has been saved to the storage.
   *
   * @var bool
   */
  protected $isNew = TRUE;

  /**
   * The data of the configuration object.
   *
   * @var array
   */
  protected $data;

  /**
   * The state of validation on this object.
   *
   * This value is set to TRUE by Config::validate() and is reset to FALSE after
   * any change to the $data variable.
   */
  protected $validated = FALSE;

  /**
   * The storage used to load and save this configuration object.
   *
   * @var ConfigStorageInterface
   */
  protected $storage;

  /**
   * The configuration context used for this configuration object.
   *
   * @var ConfigStorageInterface
   */
  protected $context;

  /**
   * Whether the configuration object has already been loaded.
   *
   * @var bool
   */
  protected $isLoaded = FALSE;

  /**
   * Constructs a configuration object.
   *
   * @param string $name
   *   The name of the configuration object being constructed.
   * @param ConfigStorageInterface $storage
   *   A storage controller object to use for reading and writing the
   *   configuration data.
   */
  public function __construct($name, ConfigStorageInterface $storage) {
    $this->name = $name;
    $this->storage = $storage;
  }

  /**
   * Initializes a configuration object.
   *
   * @return Config
   *   The configuration object.
   */
  public function init() {
    $this->isLoaded = FALSE;
    return $this;
  }

  /**
   * Initializes a configuration object with pre-loaded data.
   *
   * @param array $data
   *   Array of loaded data for this configuration object.
   *
   * @return Config
   *   The configuration object.
   */
  public function initWithData(array $data) {
    $this->isLoaded = TRUE;
    $this->isNew = FALSE;
    $this->replaceData($data);
    return $this;
  }

  /**
   * Returns the name of this configuration object.
   *
   * @return string
   *   The name of the configuration object.
   */
  public function getName() {
    return $this->name;
  }

  /**
   * Sets the name of this configuration object.
   *
   * @param string $name
   *  The name of the configuration object.
   *
   * @return Config
   *   The configuration object.
   */
  public function setName($name) {
    $this->name = $name;
    return $this;
  }

  /**
   * Validates the configuration object name.
   *
   * @param string $name
   *  The name of the configuration object.
   *
   * @throws ConfigNameException
   *
   * @see Config::MAX_NAME_LENGTH
   */
  public static function validateName($name) {
    // The name must be namespaced by owner.
    if (strpos($name, '.') === FALSE) {
      throw new ConfigNameException(format_string('Missing namespace in Config object name @name.', array(
        '@name' => $name,
      )));
    }
    // The name must be shorter than Config::MAX_NAME_LENGTH characters.
    if (strlen($name) > self::MAX_NAME_LENGTH) {
      throw new ConfigNameException(format_string('Config object name @name exceeds maximum allowed length of @length characters.', array(
        '@name' => $name,
        '@length' => self::MAX_NAME_LENGTH,
      )));
    }

    // The name must not contain any of the following characters:
    // : ? * < > " ' / \
    if (preg_match('/[:?*<>"\'\/\\\\]/', $name)) {
      throw new ConfigNameException(format_string('Invalid character in Config object name @name.', array(
        '@name' => $name,
      )));
    }
  }

  /**
   * Validate the full contents of the configuration data.
   *
   * This method is not automatically called when Config::setData() is called.
   * Because validation is a potentially expensive operation, you should call
   * this only when expecting potential problems in the provided data, such as
   * when validating user-provided imports.
   *
   * @throws ConfigValidateException
   */
  public function validateData() {
    if (!$this->validated) {
      $config_info = config_get_info($this->getName());
      module_invoke_all('config_data_validate', $this, $config_info);
      $this->validated = TRUE;
    }
  }

  /**
   * Returns whether this configuration object is new.
   *
   * @return bool
   *   TRUE if this configuration object does not exist in storage.
   */
  public function isNew() {
    if (!$this->isLoaded) {
      $this->load();
    }
    return $this->isNew;
  }

  /**
   * Gets data from this configuration object.
   *
   * @param string $key
   *   A string that maps to a key within the configuration data.
   *   For instance in the following configuration array:
   *   @code
   *   array(
   *     'foo' => array(
   *       'bar' => 'baz',
   *     ),
   *   );
   *   @endcode
   *   A key of 'foo.bar' would return the string 'baz'. However, a key of 'foo'
   *   would return array('bar' => 'baz').
   *   If no key is specified, then the entire data array is returned.
   *
   * @return mixed
   *   The data that was requested.
   */
  public function get($key = '') {
    if (!$this->isLoaded) {
      $this->load();
    }
    if (empty($key)) {
      return $this->data;
    }
    else {
      $parts = explode('.', $key);
      if (count($parts) == 1) {
        return isset($this->data[$key]) ? $this->data[$key] : NULL;
      }
      else {
        $value = $this->data;
        $key_exists = FALSE;
        foreach ($parts as $part) {
          if (is_array($value) && array_key_exists($part, $value)) {
            $value = $value[$part];
            $key_exists = TRUE;
          }
          else {
            $key_exists = FALSE;
            break;
          }
        }
        return $key_exists ? $value : NULL;
      }
    }
  }

  /**
   * Replaces the data of this configuration object.
   *
   * @param array $data
   *   The new configuration data.
   *
   * @return Config
   *   The configuration object.
   */
  public function setData(array $data) {
    $this->replaceData($data);
    // A load would destroy the data just set (for example on import).
    $this->isLoaded = TRUE;
    return $this;
  }

  /**
   * Replaces the data of this configuration object.
   *
   * This function is separate from setData() to avoid load() state tracking.
   * A load() would destroy the replaced data (for example on import). Do not
   * call set() when inside load().
   *
   * @param array $data
   *   The new configuration data.
   *
   * @return Config
   *   The configuration object.
   */
  protected function replaceData(array $data) {
    $this->data = $data;
    $this->validated = FALSE;
    return $this;
  }

  /**
   * Sets a value in this configuration object.
   *
   * Note that this will save a NULL value. If wanting to unset a key from the
   * configuration, use Config::clear($key).
   *
   * @param string $key
   *   Identifier to store value in configuration.
   * @param mixed $value
   *   Value to associate with identifier.
   *
   * @return Config
   *   The configuration object.
   */
  public function set($key, $value) {
    if (!$this->isLoaded) {
      $this->load();
    }

    // The dot/period is a reserved character; it may appear between keys, but
    // not within keys.
    $parts = explode('.', $key);
    if (count($parts) == 1) {
      $this->data[$key] = $value;
    }
    else {
      $data = &$this->data;
      $last_key = array_pop($parts);
      foreach ($parts as $part) {
        if (!isset($data)) {
          $data[$part] = array();
        }
        $data = &$data[$part];
      }
      $data[$last_key] = $value;
    }
    $this->validated = FALSE;

    return $this;
  }

  /**
   * Sets multiple values in this configuration object.
   *
   * @param array $options
   *   A keyed array of identifiers mapping to the associated values.
   *
   * @return Config
   *   The configuration object.
   */
  public function setMultiple($options) {
    foreach ($options as $key => $value) {
      $this->set($key, $value);
    }
    return $this;
  }

  /**
   * Unsets a value in this configuration object.
   *
   * @param string $key
   *   Name of the key whose value should be unset.
   *
   * @return Config
   *   The configuration object.
   */
  public function clear($key) {
    if (!$this->isLoaded) {
      $this->load();
    }
    $parts = explode('.', $key);
    if (count($parts) == 1) {
      unset($this->data[$key]);
    }
    else {
      $data = &$this->data;
      $last_key = array_pop($parts);
      foreach ($parts as $part) {
        $data = &$data[$part];
      }
      unset($data[$last_key]);
    }
    $this->validated = FALSE;
    return $this;
  }

  /**
   * Loads configuration data into this object.
   *
   * @return Config
   *   The configuration object.
   */
  public function load() {
    $this->isLoaded = FALSE;
    $data = $this->storage->read($this->name);
    if ($data === FALSE) {
      $this->isNew = TRUE;
      $this->replaceData(array());
    }
    else {
      $this->isNew = FALSE;
      $this->replaceData($data);
    }
    $this->isLoaded = TRUE;
    return $this;
  }

  /**
   * Saves the configuration object.
   *
   * @return Config
   *   The configuration object.
   */
  public function save() {
    // Validate the configuration object name before saving.
    static::validateName($this->name);
    if (!$this->isLoaded) {
      $this->load();
    }
    // Ensure config name is saved in the result.
    $this->data = array_merge(array('_config_name' => $this->name), $this->data);
    $this->storage->write($this->name, $this->data);
    $this->isNew = FALSE;

    // Empty static caches of this config file.
    if ($static = &backdrop_static('config')) {
      foreach ($static as $type => $configs) {
        if (array_key_exists($this->name, $configs)) {
          unset($static[$type][$this->name]);
        }
      }
    }

    return $this;
  }

  /**
   * Deletes the configuration object.
   *
   * @return Config
   *   The configuration object.
   */
  public function delete() {
    $this->data = array();
    $this->storage->delete($this->name);
    $this->isNew = TRUE;
    return $this;
  }

  /**
   * Retrieves the storage used to load and save this configuration object.
   *
   * @return ConfigStorageInterface
   *   The configuration storage object.
   */
  public function getStorage() {
    return $this->storage;
  }
}

/**
 * Defines an interface for configuration storage controllers.
 *
 * Classes implementing this interface allow reading and writing configuration
 * data from and to the storage.
 */
interface ConfigStorageInterface {

  /**
   * Perform any steps needed to create and initialize the storage.  This may
   * include creating directories or database tables, and initializing data
   * structures.
   *
   * This function does not re-initialize already initialized storage.
   *
   * @return bool
   *   TRUE on success, FALSE in case of an error.
   *
   * @throws ConfigStorageException
   */
  public function initializeStorage();

  /**
   * Check that the storage managed by this object is present and functional.
   *
   * @return bool
   *   TRUE on success, FALSE in case of an error.
   */
  public function isInitialized();

  /**
   * Returns whether a configuration object exists.
   *
   * @param string $name
   *   The name of a configuration object to test.
   *
   * @return bool
   *   TRUE if the configuration object exists, FALSE otherwise.
   */
  public function exists($name);

  /**
   * Reads configuration data from the storage.
   *
   * @param string $name
   *   The name of a configuration object to load.
   *
   * @throws ConfigStorageReadException
   *
   * @return array|bool
   *   The configuration data stored for the configuration object name. If no
   *   configuration data exists for the given name, FALSE is returned.
   */
  public function read($name);

  /**
   * Reads configuration data from the storage.
   *
   * @param array $name
   *   List of names of the configuration objects to load.
   *
   * @throws ConfigStorageException
   *
   * @return array
   *   A list of the configuration data stored for the configuration object name
   *   that could be loaded for the passed list of names.
   */
  public function readMultiple(array $names);

  /**
   * Writes configuration data to the storage.
   *
   * @param string $name
   *   The name of a configuration object to save.
   * @param array $data
   *   The configuration data to write.
   *
   * @return bool
   *   TRUE on success, FALSE in case of an error.
   */
  public function write($name, array $data);

  /**
   * Deletes a configuration object from the storage.
   *
   * @param string $name
   *   The name of a configuration object to delete.
   *
   * @return bool
   *   TRUE on success, FALSE otherwise.
   */
  public function delete($name);

  /**
   * Renames a configuration object in the storage.
   *
   * @param string $name
   *   The name of a configuration object to rename.
   * @param string $new_name
   *   The new name of a configuration object.
   *
   * @return bool
   *   TRUE on success, FALSE otherwise.
   */
  public function rename($name, $new_name);


  /**
   * Returns a timestamp indicating the last time a configuration was modified.
   *
   * @param string $name
   *   The name of a configuration object on which the time will be checked.
   *
   * @return int|FALSE
   *   A timestamp indicating the last time the configuration was modified or
   *   FALSE if the named configuration object doesn't exist.
   */
  public function getModifiedTime($name);

  /**
   * Encodes configuration data into the storage-specific format.
   *
   * @param array $data
   *   The configuration data to encode.
   *
   * @return string
   *   The encoded configuration data.
   *
   * This is a publicly accessible static method to allow for alternative
   * usages in data conversion scripts and also tests.
   */
  public function encode($data);

  /**
   * Decodes configuration data from the storage-specific format.
   *
   * @param string $raw
   *   The raw configuration data string to decode.
   *
   * @return array
   *   The decoded configuration data as an associative array.
   *
   * This is a publicly accessible static method to allow for alternative
   * usages in data conversion scripts and also tests.
   */
  public function decode($raw);

  /**
   * Gets configuration object names starting with a given prefix.
   *
   * Given the following configuration objects:
   * - node.type.post
   * - node.type.page
   *
   * Passing the prefix 'node.type.' will return an array containing the above
   * names.
   *
   * @param string $prefix
   *   (optional) The prefix to search for. If omitted, all configuration object
   *   names that exist are returned.
   *
   * @return array
   *   An array containing matching configuration object names.
   */
  public function listAll($prefix = '');

  /**
   * Deletes configuration objects whose names start with a given prefix.
   *
   * Given the following configuration object names:
   * - node.type.post
   * - node.type.page
   *
   * Passing the prefix 'node.type.' will delete the above configuration
   * objects.
   *
   * @param string $prefix
   *   (optional) The prefix to search for. If omitted, all configuration
   *   objects that exist will be deleted.
   *
   * @return boolean
   *   TRUE on success, FALSE otherwise.
   */
  public function deleteAll($prefix = '');

  /**
   * Import an archive of configuration files into the config storage managed
   * by this object.
   *
   * @param string $file_uri
   *   The URI of the tar archive file to import.
   *
   * @throws ConfigStorageException
   *
   * @return bool
   *   TRUE on success, FALSE otherwise.
   */
  public function importArchive($file_uri);

  /**
   * Export an archive of configuration files from the config storage managed
   * by this object.
   *
   * @param string $file_uri
   *   The URI of the tar archive file to create.
   *
   * @throws ConfigStorageException
   *
   * @return bool
   *   TRUE on success, FALSE otherwise.
   */
  public function exportArchive($file_uri);
}
