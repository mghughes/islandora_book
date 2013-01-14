<?php

/**
 * @file
 *
 * Contains one local action for ingesting pages, and one local task which
 * contains several forms for managing book objects and their pages.
 *
 * Management Forms:
 *  Create PDF.
 *  Create OCR.
 *  Create Images.
 *  Update Book Thumbnail.
 *  Reorder Pages.
 *  Delete Pages.
 */

/**
 * Local menu action, that gets the ingest page form.
 *
 * @param FedoraObject $object.
 *   The book to ingest into.
 *
 * @return sting
 *   The HTML repersentation of the ingest page form.
 */
function islandora_book_ingest_page(FedoraObject $object) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('inc', 'islandora', 'includes/ingest.form');
  return drupal_get_form('islandora_ingest_form', array(
      'book' => $object->id,
      'namespace'=> islandora_get_namespace($object->id),
      'models' => array('islandora:pageCModel'),
    )
  );
}

/**
 * Local menu task, displays all the book management forms in a set of vertical
 * tabs.
 *
 * @param FedoraObject $object
 *   The book object to manage.
 *
 * @return array
 *   A renderable array containing all the management forms related to
 *   book objects.
 */
function islandora_book_manage_book_menu(FedoraObject $object) {
  return array(
    'manage_book' => array(
      '#type' => 'vertical_tabs',
      '#weight' => 0,
      '#prefix' => '',
      'pdf' => array(
        '#access' => user_access(FEDORA_ADD_DS),
        '#title' => t('Create PDF'),
        '#type' => 'fieldset',
        'form' => drupal_get_form('islandora_book_manage_book_pdf_form', $object),
        '#collapsible' => TRUE,
        '#collapsed' => FALSE,
      ),
      'ocr' => array(
        '#access' => user_access(FEDORA_ADD_DS),
        '#title' => t('Perform OCR'),
        '#type' => 'fieldset',
        'form' => drupal_get_form('islandora_book_manage_book_ocr_form', $object),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      ),
      'images' => array(
        '#access' => user_access(FEDORA_ADD_DS),
        '#title' => t('Create Images'),
        '#type' => 'fieldset',
        'form_1' => drupal_get_form('islandora_book_manage_book_thumbnail_form', $object),
        'form_2' => drupal_get_form('islandora_book_manage_book_images_form', $object),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      ),
      'sequence' => array(
        '#access' => user_access(FEDORA_METADATA_EDIT),
        '#title' => t('Reorder Pages'),
        '#type' => 'fieldset',
        'form' => drupal_get_form('islandora_book_manage_book_sequences_form', $object),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      ),
      'delete' => array(
        '#access' => user_access(FEDORA_PURGE),
        '#title' => t('Delete Pages'),
        '#type' => 'fieldset',
        'form' => drupal_get_form('islandora_book_manage_book_delete_form', $object),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      )
    )
  );
}

/**
 * Creates a PDF of the book, by generating a PDF file for each page and then
 * combining them into a single PDF.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 * @param FedoraObject $object
 *   The object to fetch the pages from.
 *
 * @return array
 *   The Drupal form.
 */
function islandora_book_manage_book_pdf_form(array $form, array &$form_state, FedoraObject $object) {
  module_load_include('inc', 'islandora_book', 'includes/utilities');
  $form_state['object'] = $object;
  $can_derive = islandora_book_can_derive($object, 'PDF');
  return array(
    'description' => array(
      '#type' => 'item',
      '#description' => t('You must have the <b>ImageMagick</b> and <b>GhostScript</b> installed to create/combine PDF files.<br/> This will also update the PDF datastreams for each Page object.'),
    ),
    'dpi' => array(
      '#type' => 'select',
      '#title' => t('DPI - Dots Per Inch'),
      '#description' => t('Set the DPI for the generated PDF. The higher the resolution the longer the process will take.'),
      '#options' => drupal_map_assoc(array('72','96','300', '600')),
    ),
    'submit' => array(
      '#disabled' => !$can_derive,
      '#type' => 'submit',
      '#value' => t('Create PDF'),
    )
  );
}

/**
 * Triggers a batch to derive a PDF datastreams in each page object, then
 * combined them into a single PDF which is stored in the book object.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function islandora_book_manage_book_pdf_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_book', 'includes/book.batch');
  $object = $form_state['object'];
  $pages = array_keys(islandora_book_get_pages($object));
  $options = array(
    '-density' => $form_state['values']['dpi'],
    '-compress' => 'LZW'
  );
  $batch = islandora_book_create_pdf_batch($object, $pages, $options);
  batch_set($batch);
}

/**
 * Derives the OCR datastreams in each page of the book.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 * @param FedoraObject $object
 *   The object to fetch the pages from.
 *
 * @return array
 *   The Drupal form.
 */
function islandora_book_manage_book_ocr_form(array $form, array &$form_state, FedoraObject $object) {
  module_load_include('inc', 'islandora_book', 'includes/utilities');
  $form_state['object'] = $object;
  $can_derive = islandora_book_can_create_ocr();
  $can_preprocess = islandora_book_can_preprocess_ocr();
  $languages = islandora_book_get_enabled_tesseract_languages();
  return array(
    'description' => array(
      '#type' => 'item',
      '#description' => t('You must have the <b>Tesseract</b> installed to perform OCR, and <b>Gimp</b> to preprocess OCR.<br/> This will update the OCR, RAW_HOCR, and ENCODED_OCR datastreams for each Page object.')
    ),
    'language' => array(
      '#title' => t('Language'),
      '#type' => 'select',
      '#description' => t('Please select the language that the page is written in.'),
      '#options' => $languages
    ),
    'preprocess' => array(
      '#disabled' => !$can_preprocess,
      '#title' => t('Preprocessing for typescripts?'),
      '#type' => 'checkbox',
      '#description' => t('Will add additional processing for typewritten text. This script grayscales and sharpens the uploaded ingested TIFF before performing OCR processes.<br/>This has been shown to improve OCR results for some images. Use with caution. Process can be lengthly and is resource intensive.<br/>'),
      '#default_value' => $can_preprocess
    ),
    'submit' => array(
      '#disabled' => !$can_derive,
      '#type' => 'submit',
      '#value' => t('Create OCR')
    )
  );
}

/**
 * Triggers a batch to derive the OCR datastreams in each page object.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function islandora_book_manage_book_ocr_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_book', 'includes/book.batch');
  $object = $form_state['object'];
  $pages = array_keys(islandora_book_get_pages($object));
  $options = array(
    'language' => $form_state['values']['language'],
    'preprocess' => (bool) $form_state['values']['preprocess'],
  );
  $batch = islandora_book_create_ocr_batch($object, $pages, $options);
  batch_set($batch);
}

/**
 * Updates the books thumbnail from the first page in the book.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 * @param FedoraObject $object
 *   The object to update.
 *
 * @return array
 *   The Drupal form.
 */
function islandora_book_manage_book_thumbnail_form(array $form, array &$form_state, FedoraObject $object) {
  module_load_include('inc', 'islandora_book', 'includes/utilities');
  $form_state['object'] = $object;
  return array(
    'description' => array(
      '#type' => 'item',
      '#description' => t('Update the Book\'s thumbnail image. The book must have pages and the first page must have a TN datastream.')
    ),
    'submit' => array(
      '#disabled' => !islandora_book_can_update_book_thumbnail($object),
      '#type' => 'submit',
      '#value' => t('Update Thumbnail')
    )
  );
}

/**
 * The submit handler for the update thumbnail form.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function islandora_book_manage_book_thumbnail_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_book', 'includes/utilities');
  islandora_book_update_book_thumbnail($form_state['object']);
}

/**
 * Derives the image datastreams for each page object.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 * @param FedoraObject $object
 *   The object to fetch the pages from.
 *
 * @return array
 *   The Drupal form.
 */
function islandora_book_manage_book_images_form(array $form, array &$form_state, FedoraObject $object) {
  module_load_include('inc', 'islandora_book', 'includes/utilities');
  $form_state['object'] = $object;
  $can_derive = islandora_book_can_create_images();
  return array(
    'description' => array(
      '#type' => 'item',
      '#description' => t('You must have the <b>Large Image Solution Pack</b> installed to perform create images.<br/> This will update the TN, JPG, and JP2 datastreams for each Page object.')
    ),
    'submit' => array(
      '#disabled' => !$can_derive,
      '#type' => 'submit',
      '#value' => t('Create Images')
    )
  );
}

/**
 * Triggers a batch to derive image datastreams in each page object.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function islandora_book_manage_book_images_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_book', 'includes/book.batch');
  $object = $form_state['object'];
  $pages = array_keys(islandora_book_get_pages($object));
  $batch = islandora_book_create_images_batch($object, $pages);
  batch_set($batch);
}

/**
 * Gets a form that lists all the pages for the given book, giving the user the
 * option to sequence them.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 * @param FedoraObject $object
 *   The book object to fetch the pages from.
 *
 * @return array
 *   The Drupal form.
 */
function islandora_book_manage_book_sequences_form(array $form, array &$form_state, FedoraObject $object) {
  module_load_include('inc', 'islandora_book', 'includes/utilities');
  $form_state['object'] = $object;
  $pages = islandora_book_get_pages($object);
  $delta = count($pages);
  $rows = array_map(function($page) use($delta) {
            return array(
              'pid' => array('#markup' => check_plain($page['pid'])),
              'label' => array('#markup' => check_plain($page['label'])),
              'weight' => array(
                '#type' => 'weight',
                '#title' => t('Weight'),
                '#default_value' => $page['page'],
                '#delta' => $delta,
                '#title-display' => 'invisible',
              ));
          }, $pages);
  return array(
    '#tree' => TRUE,
    'table' => array(
      '#type' => 'markup',
      '#header' => array(t('PID'), t('Label'), t('Page')),
      'rows' => $rows,
    ),
    'actions' => array(
      '#type' => 'actions',
      'submit' => array(
        '#type' => 'submit',
        '#value' => t('Save Changes')
      )
    )
  );
}

/**
 * Submit handler for the sequence form.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function islandora_book_manage_book_sequences_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_book', 'includes/book.batch');
  $object = $form_state['object'];
  $pages = &$form_state['values']['table']['rows'];
  uasort($pages, 'drupal_sort_weight');
  $pages = array_keys($pages);
  $batch = islandora_book_sequence_pages_batch($object, $pages);
  batch_set($batch);
}

/**
 * Gets a form that lists all the pages for the given book, given the user the
 * option to selectively delete a number of them.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 * @param FedoraObject $object
 *   The object to fetch the pages from.
 *
 * @return array
 *   The Drupal form.
 */
function islandora_book_manage_book_delete_form(array $form, array &$form_state, FedoraObject $object) {
  module_load_include('inc', 'islandora_book', 'includes/utilities');
  $form_state['object'] = $object;
  $pages = islandora_book_get_pages($object);
  return array(
    'table' => array(
      '#type' => 'tableselect',
      '#header' => array(
        'pid' => t('PID'),
        'page' => t('Sequence Number'),
        'label' => t('Label'),
      ),
      '#options' => $pages,
      '#multiple' => TRUE,
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Delete Selected Pages'),
    )
  );
}

/**
 * Submit handler for the delete pages form.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function islandora_book_manage_book_delete_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_book', 'includes/book.batch');
  $object = $form_state['object'];
  $pages = array_values(array_filter($form_state['values']['table']));
  $batch = islandora_book_delete_pages_batch($object, $pages);
  batch_set($batch);
}