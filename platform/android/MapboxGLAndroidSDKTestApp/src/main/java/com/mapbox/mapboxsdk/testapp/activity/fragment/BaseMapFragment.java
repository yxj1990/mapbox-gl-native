package com.mapbox.mapboxsdk.testapp.activity.fragment;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapbox.mapboxsdk.maps.MapView;

import com.mapbox.mapboxsdk.testapp.R;

public abstract class BaseMapFragment extends Fragment {

  protected MapView map;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, Bundle savedInstanceState) {
    return inflater.inflate(R.layout.fragment_map, container, false);
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);
    map = (MapView) view.findViewById(R.id.mapView);
    map.onCreate(savedInstanceState);
  }

  /**
   * Called when the fragment is visible for the users.
   */
  @Override
  public void onStart() {
    super.onStart();
    map.onStart();
  }

  /**
   * Called when the fragment is ready to be interacted with.
   */
  @Override
  public void onResume() {
    super.onResume();
    map.onResume();
  }

  /**
   * Called when the fragment is pausing.
   */
  @Override
  public void onPause() {
    super.onPause();
    map.onPause();
  }

  /**
   * Called when the fragment state needs to be saved.
   *
   * @param outState The saved state
   */
  @Override
  public void onSaveInstanceState(@NonNull Bundle outState) {
    super.onSaveInstanceState(outState);
    map.onSaveInstanceState(outState);
  }

  /**
   * Called when the fragment is no longer visible for the user.
   */
  @Override
  public void onStop() {
    super.onStop();
    map.onStop();
  }

  /**
   * Called when the fragment receives onLowMemory call from the hosting Activity.
   */
  @Override
  public void onLowMemory() {
    super.onLowMemory();
    map.onLowMemory();
  }

  /**
   * Called when the fragment is view hiearchy is being destroyed.
   */
  @Override
  public void onDestroyView() {
    super.onDestroyView();
    map.onDestroy();
  }
}
