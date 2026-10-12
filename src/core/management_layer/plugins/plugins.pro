TEMPLATE = subdirs

SUBDIRS = \
    character_information_structure \
    location_information_structure \
    project_information \
    project_parameters \
    screenplay_information \
    screenplay_parameters \
    screenplay_treatment \
    screenplay_treatment_structure \
    screenplay_text \
    screenplay_text_structure \
    screenplay_statistics_structure \
    simple_text \
    simple_text_structure \
    title_page \
    recycle_bin \
    world_information_structure

exists (project_collaborators/project_collaborators.pro) {
    SUBDIRS += project_collaborators
}

exists (characters_relations/characters_relations.pro) {
    SUBDIRS += characters_relations
}
exists (character_information/character_information.pro) {
    SUBDIRS += character_information
}
exists (character_dialogues/character_dialogues.pro) {
    SUBDIRS += character_dialogues
}

exists (locations_map/locations_map.pro) {
    SUBDIRS += locations_map
}
exists (location_information/location_information.pro) {
    SUBDIRS += location_information
}
exists (location_scenes/location_scenes.pro) {
    SUBDIRS += location_scenes
}

exists (worlds_map/worlds_map.pro) {
    SUBDIRS += worlds_map
}
exists (world_information/world_information.pro) {
    SUBDIRS += world_information
}

exists (screenplay_cards/screenplay_cards.pro) {
    SUBDIRS += screenplay_cards
}
exists (screenplay_timeline/screenplay_timeline.pro) {
    SUBDIRS += screenplay_timeline
}
exists (screenplay_breakdown/screenplay_breakdown.pro) {
    SUBDIRS += screenplay_breakdown
}
exists (screenplay_breakdown_structure/screenplay_breakdown_structure.pro) {
    SUBDIRS += screenplay_breakdown_structure
}
exists (screenplay_statistics/screenplay_statistics.pro) {
    SUBDIRS += screenplay_statistics
}

exists (images_gallery/images_gallery.pro) {
    SUBDIRS += images_gallery
}

exists (mind_map/mind_map.pro) {
    SUBDIRS += mind_map
}

exists (presentation/presentation.pro) {
    SUBDIRS += presentation
}
