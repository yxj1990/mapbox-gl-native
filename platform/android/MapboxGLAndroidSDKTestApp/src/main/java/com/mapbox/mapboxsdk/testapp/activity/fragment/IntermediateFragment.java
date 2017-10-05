package com.mapbox.mapboxsdk.testapp.activity.fragment;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

public class IntermediateFragment extends Fragment{

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
    TextView textView = new TextView(inflater.getContext());
    textView.setText("Hello World");
    textView.setTextSize(24);
    textView.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        ((MapFragmentActivity)getActivity()).replaceFragment(new SecondFragment());
      }
    });
    return textView;
  }
}
